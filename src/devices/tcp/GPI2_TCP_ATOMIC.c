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
#include "GASPI.h"
#include "GPI2_TCP.h"

gaspi_return_t
pgaspi_dev_atomic_fetch_add (const gaspi_segment_id_t segment_id,
			     const gaspi_offset_t offset,
			     const gaspi_rank_t rank,
			     const gaspi_atomic_value_t val_add)
{

  tcp_dev_wr_t wr =
    {
      .wr_id       = rank,
      .cq_handle   = glb_gaspi_ctx_tcp.scqGroups->num,      
      .source      = glb_gaspi_ctx.rank,
      .target      = rank,
      .local_addr  = (uintptr_t) (glb_gaspi_ctx.nsrc.data.buf),
      .remote_addr = glb_gaspi_ctx.rrmd[segment_id][rank].data.addr + offset,
      .length      = sizeof(gaspi_atomic_value_t),
      .swap        = 0,
      .compare_add = val_add,
      .opcode      = POST_ATOMIC_FETCH_AND_ADD
    } ;
  
  if( write(glb_gaspi_ctx_tcp.qpGroups->handle, &wr, sizeof(tcp_dev_wr_t)) < (ssize_t) sizeof(tcp_dev_wr_t) )
    {
      return GASPI_ERROR;
    }

  //TODO: repeated code (what changes is the ctx)
  glb_gaspi_ctx.ne_count_grp++; 

  int ne = 0;
  int i;
  tcp_dev_wc_t wc;
  
  for (i = 0; i < glb_gaspi_ctx.ne_count_grp; i++)
    {
      do
	{
	  ne = tcp_dev_return_wc (glb_gaspi_ctx_tcp.scqGroups, &wc);
	}
      while (ne == 0);

      if( (ne < 0) || (wc.status != TCP_WC_SUCCESS) )
	{
	  return GASPI_ERROR;
	}
    }

  //TODO: repeated code (what changes is the ctx)
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
  tcp_dev_wr_t wr =
    {
      .wr_id       = rank,
      .cq_handle   = glb_gaspi_ctx_tcp.scqGroups->num,      
      .source      = glb_gaspi_ctx.rank,
      .target       = rank,
      .local_addr  = (uintptr_t) (glb_gaspi_ctx.nsrc.data.buf),
      .remote_addr = glb_gaspi_ctx.rrmd[segment_id][rank].data.addr + offset,
      .length      = sizeof(gaspi_atomic_value_t),
      .swap        = val_new,
      .compare_add = comparator,
      .opcode      = POST_ATOMIC_CMP_AND_SWP
    } ;
  
  if( write(glb_gaspi_ctx_tcp.qpGroups->handle, &wr, sizeof(tcp_dev_wr_t)) < (ssize_t) sizeof(tcp_dev_wr_t) )
    {
      return GASPI_ERROR;
    }

  //TODO: repeated code (what changes is the ctx)
  glb_gaspi_ctx.ne_count_grp++;

  int ne = 0;
  int i;
  tcp_dev_wc_t wc;
  
  for (i = 0; i < glb_gaspi_ctx.ne_count_grp; i++)
    {
      do
	{
	  ne = tcp_dev_return_wc (glb_gaspi_ctx_tcp.scqGroups, &wc);
	}
      while (ne == 0);

      if ((ne < 0) || (wc.status != TCP_WC_SUCCESS))
	{
	  return GASPI_ERROR;
	}
    }

  //TODO: repeated code (what changes is the ctx)
  glb_gaspi_ctx.ne_count_grp = 0;
  
  return GASPI_SUCCESS;
}
  
