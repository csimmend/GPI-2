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
#include "GPI2.h"
#include "GASPI.h"
#include "GPI2_IB.h"

gaspi_return_t
pgaspi_dev_atomic_fetch_add (const gaspi_segment_id_t segment_id,
			     const gaspi_offset_t offset,
			     const gaspi_rank_t rank,
			     const gaspi_atomic_value_t val_add)
{
  int i;
  struct ibv_send_wr *bad_wr;
  struct ibv_sge slist;
  struct ibv_send_wr swr;

  slist.addr = (uintptr_t) (glb_gaspi_ctx.nsrc.data.buf);
  slist.length = sizeof(gaspi_atomic_value_t);
  slist.lkey = ((struct ibv_mr *) glb_gaspi_ctx.nsrc.mr[0])->lkey;

  swr.wr.atomic.remote_addr = glb_gaspi_ctx.rrmd[segment_id][rank].data.addr + offset;

  swr.wr.atomic.rkey = glb_gaspi_ctx.rrmd[segment_id][rank].rkey[0];
  swr.wr.atomic.compare_add = val_add;

  swr.wr_id = rank;
  swr.sg_list = &slist;
  swr.num_sge = 1;
  swr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
  swr.send_flags = IBV_SEND_SIGNALED;
  swr.next = NULL;

  if (ibv_post_send (glb_gaspi_ctx_ib.qpGroups[rank], &swr, &bad_wr))
    {
      return GASPI_ERROR;
    }

  //TODO
  glb_gaspi_ctx.ne_count_grp++;

  int ne = 0;
  for (i = 0; i < glb_gaspi_ctx.ne_count_grp; i++)
    {
      do
	{
	  ne = ibv_poll_cq (glb_gaspi_ctx_ib.scqGroups, 1,
			    glb_gaspi_ctx_ib.wc_grp_send);
	}
      while (ne == 0);

      if ((ne < 0)  || (glb_gaspi_ctx_ib.wc_grp_send[i].status != IBV_WC_SUCCESS))
	{
	  return GASPI_ERROR;
	}
    }

  //TODO: remove from here
  glb_gaspi_ctx.ne_count_grp = 0;

  return GASPI_SUCCESS;
}


gaspi_return_t
pgaspi_dev_atomic_compare_swap (const gaspi_segment_id_t segment_id,
				const gaspi_offset_t offset,
				const gaspi_rank_t rank,
				const gaspi_atomic_value_t comparator,
				const gaspi_atomic_value_t val_new)
{ 
  struct ibv_send_wr *bad_wr;
  struct ibv_sge slist;
  struct ibv_send_wr swr;
  int i;

  slist.addr = (uintptr_t) (glb_gaspi_ctx.nsrc.data.buf);
  slist.length = sizeof(gaspi_atomic_value_t);
  slist.lkey = ((struct ibv_mr *) glb_gaspi_ctx.nsrc.mr[0])->lkey;
  
  swr.wr.atomic.remote_addr = glb_gaspi_ctx.rrmd[segment_id][rank].data.addr + offset;

  swr.wr.atomic.rkey = glb_gaspi_ctx.rrmd[segment_id][rank].rkey[0];
  swr.wr.atomic.compare_add = comparator;
  swr.wr.atomic.swap = val_new;

  swr.wr_id = rank;
  swr.sg_list = &slist;
  swr.num_sge = 1;
  swr.opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
  swr.send_flags = IBV_SEND_SIGNALED;
  swr.next = NULL;

  if (ibv_post_send (glb_gaspi_ctx_ib.qpGroups[rank], &swr, &bad_wr))
    {
      return GASPI_ERROR;
    }

  glb_gaspi_ctx.ne_count_grp++;

  int ne = 0;
  for (i = 0; i < glb_gaspi_ctx.ne_count_grp; i++)
    {
      do
	{
	  ne = ibv_poll_cq (glb_gaspi_ctx_ib.scqGroups, 1,
			    glb_gaspi_ctx_ib.wc_grp_send);
	}
      while (ne == 0);

      if ((ne < 0) || (glb_gaspi_ctx_ib.wc_grp_send[i].status != IBV_WC_SUCCESS))
	{
	  return GASPI_ERROR;
	}
    }

  //TODO:
  glb_gaspi_ctx.ne_count_grp = 0;
  
  return GASPI_SUCCESS;
}
