 /*
Copyright (c) Fraunhofer ITWM - Carsten Lojewski <lojewski@itwm.fhg.de>, 2013-2016

This file is part of GPI-2.

GPI-2 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

GPI-2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GPI-2. If not, see <http://www.gnu.org/licenses/>.
*/
#include <stddef.h>
#include <sys/timeb.h>
#include <unistd.h>

#include "PGASPI.h"
#include "GPI2.h"
#include "GPI2_Dev.h"
#include "GPI2_Utility.h"
#include "GPI2_SN.h"

#pragma weak gaspi_segment_max = pgaspi_segment_max
gaspi_return_t
pgaspi_segment_max (gaspi_number_t * const segment_max)
{
  gaspi_verify_null_ptr(segment_max);

  *segment_max = GASPI_MAX_MSEGS;

  return GASPI_SUCCESS;
}

#pragma weak gaspi_segment_size = pgaspi_segment_size
gaspi_return_t
pgaspi_segment_size (const gaspi_segment_id_t segment_id,
		     const gaspi_rank_t rank,
		     gaspi_size_t * const size)
{
  gaspi_verify_init("gaspi_segment_size");
  gaspi_verify_segment(segment_id);
  gaspi_verify_null_ptr(glb_gaspi_ctx.rrmd[segment_id]);
  gaspi_verify_null_ptr(size);

  gaspi_size_t seg_size = glb_gaspi_ctx.rrmd[segment_id][rank].size;

  gaspi_verify_segment_size(seg_size);

  *size = seg_size;

  return GASPI_SUCCESS;
}

#pragma weak gaspi_segment_ptr = pgaspi_segment_ptr
gaspi_return_t
pgaspi_segment_ptr (const gaspi_segment_id_t segment_id, gaspi_pointer_t * ptr)
{
  gaspi_verify_init("gaspi_segment_ptr");
  gaspi_verify_segment(segment_id);
  gaspi_verify_null_ptr(glb_gaspi_ctx.rrmd[segment_id]);
  gaspi_verify_null_ptr(ptr);

  gaspi_verify_segment_size(glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].size);

#ifdef GPI2_CUDA
  if(glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].cudaDevId >= 0)
    *ptr = glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.buf;
  else
#endif

    *ptr = glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.buf;

  return GASPI_SUCCESS;
}

#pragma weak gaspi_segment_list = pgaspi_segment_list
gaspi_return_t
pgaspi_segment_list (const gaspi_number_t num,
		     gaspi_segment_id_t * const segment_id_list)
{
  int i, idx = 0;

  gaspi_verify_init("gaspi_segment_list");
  gaspi_verify_null_ptr(segment_id_list);

  for (i = 0; i < GASPI_MAX_MSEGS; i++)
    {
      if(glb_gaspi_ctx.rrmd[(gaspi_segment_id_t) i] != NULL )
	if(glb_gaspi_ctx.rrmd[(gaspi_segment_id_t) i][glb_gaspi_ctx.rank].trans)
	  segment_id_list[idx++] = (gaspi_segment_id_t) i;
    }

  if (idx != glb_gaspi_ctx.mseg_cnt)
    {
      return GASPI_ERROR;
    }

  return GASPI_SUCCESS;
}

#pragma weak gaspi_segment_num = pgaspi_segment_num
gaspi_return_t
pgaspi_segment_num (gaspi_number_t * const segment_num)
{
  gaspi_verify_init("gaspi_segment_num");
  gaspi_verify_null_ptr(segment_num);

  *segment_num = (gaspi_number_t) glb_gaspi_ctx.mseg_cnt;
  return GASPI_SUCCESS;
}

#pragma weak gaspi_segment_avail_local = pgaspi_segment_avail_local
gaspi_return_t
pgaspi_segment_avail_local (gaspi_segment_id_t * const avail_seg_id)
{
  gaspi_verify_null_ptr(avail_seg_id);

  gaspi_number_t num_segs;
  if( gaspi_segment_num (&num_segs) != GASPI_SUCCESS)
    {
      return GASPI_ERROR;
    }

  //fast path
  if( num_segs == 0 )
    {
      *avail_seg_id = 0;
      return GASPI_SUCCESS;
    }

  gaspi_number_t segs_max;
  if( gaspi_segment_max (&segs_max) != GASPI_SUCCESS )
    {
      return GASPI_ERROR;
    }

  if( num_segs == segs_max)
    {
      return GASPI_ERR_MANY_SEG;
    }

  gaspi_segment_id_t *segment_ids = malloc(num_segs * sizeof(gaspi_segment_id_t));
  if( segment_ids == NULL )
    {
      return GASPI_ERR_MEMALLOC;
    }

  if( gaspi_segment_list (num_segs, segment_ids) != GASPI_SUCCESS)
    {
      free(segment_ids);
      return GASPI_ERROR;
    }

  gaspi_number_t i;
  for(i = 1; i < num_segs; i++)
    {
      if( segment_ids[i] != segment_ids[i-1]+1 )
	{
	  *avail_seg_id = (gaspi_segment_id_t) i ;
	  free(segment_ids);
	  return GASPI_SUCCESS;
	}
    }
  *avail_seg_id = num_segs;

  free(segment_ids);

  return GASPI_SUCCESS;
}

#pragma weak gaspi_segment_alloc = pgaspi_segment_alloc
gaspi_return_t
pgaspi_segment_alloc (const gaspi_segment_id_t segment_id,
		      const gaspi_size_t size,
		      const gaspi_alloc_t alloc_policy)
{
  gaspi_verify_init("gaspi_segment_alloc");
  gaspi_verify_segment_size(size);
  gaspi_verify_segment(segment_id);

  if (glb_gaspi_ctx.mseg_cnt >= GASPI_MAX_MSEGS)
    {
      return GASPI_ERR_MANY_SEG;
    }

  lock_gaspi_tout (&gaspi_mseg_lock, GASPI_BLOCK);

  gaspi_return_t eret = GASPI_ERROR;

  /*  TODO: for now like this, but we need to change this */
#ifndef GPI2_CUDA

  if( glb_gaspi_ctx.rrmd[segment_id] == NULL)
    {
      glb_gaspi_ctx.rrmd[segment_id] = (gaspi_rc_mseg *) calloc (glb_gaspi_ctx.tnc, sizeof (gaspi_rc_mseg));

      if(!glb_gaspi_ctx.rrmd[segment_id])
	{
	  eret = GASPI_ERR_MEMALLOC;
	  goto endL;
	}
    }

  if( glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].size)
    {
      eret = GASPI_SUCCESS;
      goto endL;
    }

  const long page_size = sysconf (_SC_PAGESIZE);

  if( page_size < 0 )
    {
      gaspi_print_error ("Failed to get system's page size.");
      goto endL;
    }

  if( posix_memalign ((void **) &glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.ptr,
		      page_size,
		      size + NOTIFY_OFFSET) != 0)
    {
      gaspi_print_error ("Memory allocation (posix_memalign) failed");
      eret = GASPI_ERR_MEMALLOC;
      goto endL;
    }

  memset (glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.ptr, 0, NOTIFY_OFFSET);

  if( alloc_policy == GASPI_MEM_INITIALIZED)
    {
      memset (glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.ptr, 0, size + NOTIFY_OFFSET);
    }

  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].size = size;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].notif_spc_size = NOTIFY_OFFSET;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].notif_spc.addr = glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.addr;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.addr += NOTIFY_OFFSET;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].user_provided = 0;

  if(pgaspi_dev_register_mem(&(glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank]), size + NOTIFY_OFFSET) < 0)
    {
      goto endL;
    }

#else
  eret = pgaspi_dev_segment_alloc(segment_id, size, alloc_policy);
  if( eret != GASPI_SUCCESS )
    goto endL;
#endif /* GPI2_CUDA */

  glb_gaspi_ctx.mseg_cnt++;

  eret = GASPI_SUCCESS;
 endL:
  unlock_gaspi (&gaspi_mseg_lock);
  return eret;

}

#pragma weak gaspi_segment_delete = pgaspi_segment_delete
gaspi_return_t
pgaspi_segment_delete (const gaspi_segment_id_t segment_id)
{
  gaspi_verify_init("gaspi_segment_delete");
  gaspi_verify_segment(segment_id);
  gaspi_verify_null_ptr(glb_gaspi_ctx.rrmd[segment_id]);

  gaspi_verify_segment_size(glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].size);

  gaspi_return_t eret = GASPI_ERROR;

  lock_gaspi_tout(&gaspi_mseg_lock, GASPI_BLOCK);

  /*  TODO: for now like this but we need a better solution */
#ifdef GPI2_CUDA
  eret = pgaspi_dev_segment_delete(segment_id);
#else
  if(pgaspi_dev_unregister_mem(&(glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank])) < 0)
    {
      unlock_gaspi (&gaspi_mseg_lock);
      return GASPI_ERR_DEVICE;
    }

  /* For both "normal" and user-provided segments, the notif_spc
     points to begin of memory and only the size changes.
  */
  free (glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].notif_spc.buf);

  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].data.buf = NULL;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].notif_spc.buf = NULL;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].size = 0;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].notif_spc_size = 0;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].trans = 0;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].mr[0] = NULL;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].mr[1] = NULL;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].rkey[0] = 0;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].rkey[1] = 0;
  glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].user_provided = 0;

  /* Reset trans info flag for all ranks */
  int r;
  for(r = 0; r < glb_gaspi_ctx.tnc; r++)
    {
      glb_gaspi_ctx.rrmd[segment_id][r].trans = 0;
    }

  eret = GASPI_SUCCESS;
#endif

  glb_gaspi_ctx.mseg_cnt--;

  unlock_gaspi (&gaspi_mseg_lock);

  return eret;
}

#pragma weak gaspi_segment_register = pgaspi_segment_register
gaspi_return_t
pgaspi_segment_register(const gaspi_segment_id_t segment_id,
			const gaspi_rank_t rank,
			const gaspi_timeout_t timeout_ms)
{
  gaspi_verify_init("gaspi_segment_register");
  gaspi_verify_segment(segment_id);
  gaspi_verify_null_ptr(glb_gaspi_ctx.rrmd[segment_id]);
  gaspi_verify_rank(rank);

  gaspi_verify_segment_size(glb_gaspi_ctx.rrmd[segment_id][glb_gaspi_ctx.rank].size);

  if( rank == glb_gaspi_ctx.rank)
    {
      glb_gaspi_ctx.rrmd[segment_id][rank].trans = 1;
      return GASPI_SUCCESS;
    }

  if(lock_gaspi_tout(&glb_gaspi_ctx_lock, timeout_ms))
    {
      return GASPI_TIMEOUT;
    }

  gaspi_return_t eret = gaspi_sn_command(GASPI_SN_SEG_REGISTER, rank, timeout_ms, (void *) &segment_id);

  glb_gaspi_ctx.rrmd[segment_id][rank].trans = 1;

  unlock_gaspi(&glb_gaspi_ctx_lock);

  return eret;
}

/* TODO: from the spec: */
/* 1) If the communication infrastructure was not established */
/* for all group members beforehand, gaspi_segment_create will accomplish this */
/* as well. */

/* 2) Creating a new segment with an existing segment ID */
/* results in undefined behavior */
#pragma weak gaspi_segment_create = pgaspi_segment_create
gaspi_return_t
pgaspi_segment_create(const gaspi_segment_id_t segment_id,
		      const gaspi_size_t size,
		      const gaspi_group_t group,
		      const gaspi_timeout_t timeout_ms,
		      const gaspi_alloc_t alloc_policy)
{
  gaspi_verify_group(group);

  gaspi_return_t eret = pgaspi_segment_alloc (segment_id, size, alloc_policy);

  if(eret != GASPI_SUCCESS)
    {
      return eret;
    }

  /* register segment to all other group members */
  int r;
  for(r = 1; r <= glb_gaspi_group_ctx[group].tnc; r++)
    {
      int i = (glb_gaspi_group_ctx[group].rank + r) % glb_gaspi_group_ctx[group].tnc;

      eret = pgaspi_segment_register(segment_id,
				     glb_gaspi_group_ctx[group].rank_grp[i],
				     timeout_ms);
      if( eret != GASPI_SUCCESS )
	{
	  return eret;
	}
    }

  eret = pgaspi_barrier(group, timeout_ms);

  return eret;
}

/* Extensions */
/* TODO: */
/* - GPU case */
/* - merge common/repetead code from other segment related function (create, alloc, ...) */
/* - check/deal with alignment issues */

#pragma weak gaspi_segment_bind = pgaspi_segment_bind
gaspi_return_t
pgaspi_segment_bind ( gaspi_segment_id_t const segment_id,
		      gaspi_pointer_t const pointer,
		      gaspi_size_t const size,
		      gaspi_memory_description_t const memory_description)
{
  gaspi_verify_init("gaspi_segment_bind");
  gaspi_verify_segment_size(size);
  gaspi_verify_segment(segment_id);

  gaspi_context* const ctx = &glb_gaspi_ctx;
  const int myrank = (int) ctx->rank;
  long page_size;

  if( ctx->mseg_cnt >= GASPI_MAX_MSEGS )
    {
      return GASPI_ERR_MANY_SEG;
    }

  lock_gaspi_tout (&gaspi_mseg_lock, GASPI_BLOCK);

  gaspi_return_t eret = GASPI_ERROR;

  if( ctx->rrmd[segment_id] == NULL )
    {
      ctx->rrmd[segment_id] = (gaspi_rc_mseg *) calloc (ctx->tnc, sizeof (gaspi_rc_mseg));

      if( ctx->rrmd[segment_id] == NULL )
	{
	  eret = GASPI_ERR_MEMALLOC;
	  goto endL;
	}
    }

  if( ctx->rrmd[segment_id][myrank].size)
    {
      eret = GASPI_SUCCESS;
      goto endL;
    }

  /* Get space for notifications and register it as well */
  page_size = sysconf (_SC_PAGESIZE);

  if( page_size < 0 )
    {
      gaspi_print_error ("Failed to get system's page size.");
      goto endL;
    }

  if( posix_memalign( (void **) &ctx->rrmd[segment_id][myrank].notif_spc.ptr,
		      page_size,
		      NOTIFY_OFFSET) != 0 )
    {
      gaspi_print_error ("Memory allocation (posix_memalign) failed");
      eret = GASPI_ERR_MEMALLOC;
      goto endL;
    }

  memset (ctx->rrmd[segment_id][myrank].notif_spc.ptr, 0, NOTIFY_OFFSET);

  /* Set the segment data pointer and size */
  ctx->rrmd[segment_id][myrank].data.ptr = pointer;
  ctx->rrmd[segment_id][myrank].size = size;
  ctx->rrmd[segment_id][myrank].notif_spc_size = NOTIFY_OFFSET;

  ctx->rrmd[segment_id][glb_gaspi_ctx.rank].user_provided = 1;

  /* TODO: what to do with the memory description?? */
  ctx->rrmd[segment_id][myrank].desc = memory_description;

  /* Register it with the device */
  if( pgaspi_dev_register_mem( &(ctx->rrmd[segment_id][myrank]), size) < 0)
    {
      eret = GASPI_ERR_DEVICE;
      goto endL;
    }


  ctx->mseg_cnt++;

  eret = GASPI_SUCCESS;

 endL:
  unlock_gaspi (&gaspi_mseg_lock);
  return eret;
}

#pragma weak gaspi_segment_use = pgaspi_segment_use
gaspi_return_t
pgaspi_segment_use ( gaspi_segment_id_t const segment_id,
		     gaspi_pointer_t const pointer,
		     gaspi_size_t const size,
		     gaspi_group_t const group,
		     gaspi_timeout_t const timeout,
		     gaspi_memory_description_t const memory_description)
{
  gaspi_return_t ret = pgaspi_segment_bind(segment_id, pointer, size, memory_description );
  if( GASPI_SUCCESS != ret )
    {
      return ret;
    }

  gaspi_number_t group_size;
  ret = pgaspi_group_size( group, &group_size );
  if( GASPI_SUCCESS != ret )
    {
      return ret;
    }

  gaspi_rank_t *group_ranks = malloc( group_size * sizeof(gaspi_rank_t) );
  if( group_ranks == NULL )
    {
      return GASPI_ERR_MEMALLOC;
    }

  ret = gaspi_group_ranks (group, group_ranks);
  if( GASPI_SUCCESS != ret )
    {
      free(group_ranks);
      return ret;
    }

  gaspi_rank_t i;
  for(i = 0; i < group_size; i++ )
    {
      ret = pgaspi_segment_register( segment_id, group_ranks[i], timeout);
      if ( GASPI_SUCCESS != ret )
	{
	  free(group_ranks);
	  return ret;
	}
    }

  free(group_ranks);

  return gaspi_barrier( group, timeout);
}
