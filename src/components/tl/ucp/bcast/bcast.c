/**
 * Copyright (C) Mellanox Technologies Ltd. 2021.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */
#include "config.h"
#include "tl_ucp.h"
#include "bcast.h"

#ifdef HAVE_DPU_OFFLOAD
#include "dpu_offload_service_daemon.h"
#include "bcast_offload.h"

static bool active_colls_initialized = false;

ucc_base_coll_alg_info_t
    ucc_tl_ucp_bcast_algs[UCC_TL_UCP_BCAST_ALG_LAST + 1] = {
        [UCC_TL_UCP_BCAST_ALG_KNOMIAL] =
            {.id   = UCC_TL_UCP_BCAST_ALG_KNOMIAL,
             .name = "knomial",
             .desc = "bcast over knomial tree with arbitrary radix "
                     "(latency oriented alg)"},
        [UCC_TL_UCP_BCAST_ALG_SAG_KNOMIAL] =
            {.id   = UCC_TL_UCP_BCAST_ALG_SAG_KNOMIAL,
             .name = "sag_knomial",
             .desc = "recursive k-nomial scatter followed by k-nomial "
                     "bcast (bw oriented alg)"},
        [UCC_TL_UCP_BCAST_ALG_OFFLOAD] =
            {.id   = UCC_TL_UCP_BCAST_ALG_OFFLOAD,
             .name = "offload",
             .desc = "offloaded knomial algorithm running on the dpu"},
        [UCC_TL_UCP_BCAST_ALG_LAST] = {
            .id = 0, .name = NULL, .desc = NULL}};

void
get_buffer_range_b(ucc_coll_args_t *args, ucc_coll_buffer_info_t *info,
                 ucc_rank_t size, void **start, size_t *len)
{
    size_t dt_size = ucc_dt_size(info->datatype);
    int start_offset_g, end_offset_g;

    start_offset_g = 0;
    end_offset_g = info->count * dt_size;

    *len = end_offset_g - start_offset_g;
    *start = info->buffer + start_offset_g;
}

ucc_status_t
register_memh_b(ucc_tl_ucp_task_t *task, void *address, size_t length,
              ucp_mem_h *memh)
{
    ucc_tl_ucp_lib_t *lib        = TASK_LIB(task);
    ucp_context_h ucp_context    = TASK_CTX(task)->ucp_context;
    ucp_mem_map_params_t mparams = {0};
    int rc;

    mparams.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                         UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                         UCP_MEM_MAP_PARAM_FIELD_FLAGS;
    mparams.flags      = UCP_MEM_MAP_SHARED;
    mparams.address    = address;
    mparams.length     = length;

    rc = ucp_mem_map(ucp_context, &mparams, memh);
    if (rc) {
        tl_error(lib, "ucp_mem_map failed: %s", ucs_status_string(rc));
        return UCC_ERR_NO_MEMORY;
    }

    return UCC_OK;
}

ucc_status_t pack_rkey_b(ucc_tl_ucp_task_t *task, ucp_mem_h memh, void **rkey_buf,
                       size_t *buf_size)
{
    ucc_tl_ucp_lib_t *lib = TASK_LIB(task);
    ucp_context_h context = TASK_CTX(task)->ucp_context;
    ucp_mkey_pack_params_t mkey_pack_params = {0};
    int rc;

    mkey_pack_params.field_mask = UCP_MKEY_PACK_PARAM_FIELD_FLAGS;
    mkey_pack_params.flags      = UCP_MKEY_PACK_FLAG_SHARED;
    rc = ucp_mkey_pack(context, memh, &mkey_pack_params, rkey_buf, buf_size);
    if (rc) {
        tl_error(lib, "ucp_mkey_pack failed: %s", ucs_status_string(rc));
        return UCC_ERR_NO_MEMORY;
    }

    return UCC_OK;
}

size_t
get_offload_args_packed_size_b(ucc_rank_t comm_size, size_t s_rkey_buf_size,
                             size_t r_rkey_buf_size)
{
    size_t total_size = 0;

    total_size += FIELD_SIZEOF(bcast_offload_args_t, coll_type);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, tag);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, group_id);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, size);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, rank);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, padding);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, s_start);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, s_length);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, r_start);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, r_length);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, r_count);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, s_rkey_len);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, r_rkey_len);
    total_size += s_rkey_buf_size;
    total_size += r_rkey_buf_size;

    return total_size;
}

size_t
pack_bcast_offload_args(ucc_tl_ucp_task_t *task, void *s_start,
                             size_t s_length, void *r_start, size_t r_length,
                             void *s_rkey_buf, size_t s_rkey_buf_len,
                             void *r_rkey_buf, size_t r_rkey_buf_len,
                             void *args_buf)
{
    ucc_tl_ucp_team_t *team = TASK_TEAM(task);
    ucc_coll_args_t   *args = &TASK_ARGS(task);
    size_t total_size = 0;
    size_t r_dt_size = ucc_dt_size(args->dst.info.datatype);

    *((uint32_t *)(args_buf + total_size)) = args->coll_type;
    total_size += FIELD_SIZEOF(bcast_offload_args_t, coll_type);
    *((uint32_t *)(args_buf + total_size)) = task->tag;
    total_size += FIELD_SIZEOF(bcast_offload_args_t, tag);
    *((uint32_t *)(args_buf + total_size)) = UCC_TL_TEAM_ID(team);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, group_id);
    *((uint32_t *)(args_buf + total_size)) = UCC_TL_TEAM_SIZE(team);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, size);
    *((uint32_t *)(args_buf + total_size)) = UCC_TL_TEAM_RANK(team);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, rank);
    total_size += FIELD_SIZEOF(bcast_offload_args_t, padding);

    *((uint64_t *)(args_buf + total_size)) = (uint64_t)s_start;
    total_size += FIELD_SIZEOF(bcast_offload_args_t, s_start);
    *((uint64_t *)(args_buf + total_size)) = s_length;
    total_size += FIELD_SIZEOF(bcast_offload_args_t, s_length);

    *((uint64_t *)(args_buf + total_size)) = (uint64_t)r_start;
    total_size += FIELD_SIZEOF(bcast_offload_args_t, r_start);
    *((uint64_t *)(args_buf + total_size)) = r_length;
    total_size += FIELD_SIZEOF(bcast_offload_args_t, r_length);

    *((uint64_t *)(args_buf + total_size)) = args->dst.info.count * r_dt_size;
    total_size += sizeof(uint64_t);

    *((uint64_t *)(args_buf + total_size)) = s_rkey_buf_len;
    total_size += sizeof(uint64_t);
    *((uint64_t *)(args_buf + total_size)) = r_rkey_buf_len;
    total_size += sizeof(uint64_t);

    memcpy(args_buf + total_size, s_rkey_buf, s_rkey_buf_len);
    total_size += s_rkey_buf_len;
    memcpy(args_buf + total_size, r_rkey_buf, r_rkey_buf_len);
    total_size += r_rkey_buf_len;

    return total_size;
}

ucc_status_t ucc_tl_ucp_bcast_offload_progress(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t      *task = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t      *team = TASK_TEAM(task);
    ucc_tl_ucp_lib_t       *lib  = TASK_LIB(task);
    bcast_host_coll_t  *op   = NULL, *op_item, *op_tmp;

    /* find the associated coll op from active_colls */
    ucc_list_for_each_safe(op_item, op_tmp, &active_colls, super) {
        if (op_item->coll_task == coll_task) {
            /* found a match */
            op = op_item;
            break;
        }
    }
    assert(op);

    if (1 == op->complete) {
        ucc_assert(UCC_TL_UCP_TASK_P2P_COMPLETE(task));
        task->super.super.status = UCC_OK;

        /* deregister xgvmi mkey at end of coll */
        ucp_context_h ucp_context = TASK_CTX(task)->ucp_context;
        int rc;

        rc = ucp_mem_unmap(ucp_context, op->s_memh);
        if (rc) {
            tl_error(lib, "ucp_mem_unmap failed: %s", ucs_status_string(rc));
        }
        rc = ucp_mem_unmap(ucp_context, op->r_memh);
        if (rc) {
            tl_error(lib, "ucp_mem_unmap failed: %s", ucs_status_string(rc));
        }

        /* clean up op since it's no longer needed */
        ucs_list_del(&op->super);
        free(op);
    } else {
        /* progress offload engine */
        execution_context_t *econtext = team->dpu_offloading_econtext;
        econtext->progress(econtext);
    }

    return task->super.super.status;
}

ucc_status_t ucc_tl_ucp_bcast_offload_start(ucc_coll_task_t *coll_task)
{
    ucc_tl_ucp_task_t      *task = ucc_derived_of(coll_task, ucc_tl_ucp_task_t);
    ucc_tl_ucp_team_t      *team = TASK_TEAM(task);
    ucc_tl_ucp_lib_t       *lib  = TASK_LIB(task);
    ucc_coll_args_t        *args = &TASK_ARGS(task);
    bcast_host_coll_t  *op   = NULL, *op_item, *op_tmp;
    ucc_status_t            status;
    int                     rc;

    ucc_tl_ucp_task_reset(task);

    /* find the associated coll op from active_colls */
    ucc_list_for_each_safe(op_item, op_tmp, &active_colls, super) {
        if (op_item->coll_task == coll_task) {
            /* found a match */
            op = op_item;
            break;
        }
    }
    assert(op);
    
    //发送缓冲区的起始位置和长度
    /* get the true start address and buffer length */
    void *s_start = args->src.info.buffer;
    size_t s_len = ucc_dt_size(args->src.info.datatype) * args->src.info.count;

    //接受缓冲区，和dst的缓冲区信息对应
    void *r_start = NULL;
    size_t r_len = 0;
    get_buffer_range_b(args, &(args->dst.info), UCC_TL_TEAM_SIZE(team),
                     &r_start, &r_len);
    //重新修改buffer的开始地址
    assert(args->dst.info.buffer == r_start);

    /* register xgvmi mkeys */
    status = register_memh_b(task, s_start, s_len, &op->s_memh);
    if (status) {
        return UCC_ERR_NO_MEMORY;
    }
    status = register_memh_b(task, r_start, r_len, &op->r_memh);
    if (status) {
        return UCC_ERR_NO_MEMORY;
    }

    /* pack rkey buffers */
    void *s_rkey_buf, *r_rkey_buf;
    size_t s_rkey_buf_len, r_rkey_buf_len;
    status = pack_rkey_b(task, op->s_memh, &s_rkey_buf, &s_rkey_buf_len);
    if (status) {
        return UCC_ERR_NO_MEMORY;
    }
    status = pack_rkey_b(task, op->r_memh, &r_rkey_buf, &r_rkey_buf_len);
    if (status) {
        return UCC_ERR_NO_MEMORY;
    }

    /* calculate buffer size for metadata to send to DPU */
    size_t packed_size = get_offload_args_packed_size_b(
            UCC_TL_TEAM_SIZE(team), s_rkey_buf_len, r_rkey_buf_len);

    /* get an event from event pool, allocate payload buffer */
    execution_context_t *econtext = team->dpu_offloading_econtext;
    dpu_offload_event_info_t event_info = { .payload_size = packed_size, };
    dpu_offload_event_t *event;
    rc = event_get(econtext->event_channels, &event_info, &event);
    if (rc || !event) {
        tl_error(lib, "event_get() failed");
        return UCC_ERR_NO_MESSAGE;
    }

    /* pack offload args to event payload buffer */
    size_t offload_args_packed_size =
        pack_bcast_offload_args(task, s_start, s_len, r_start, r_len,
        s_rkey_buf, s_rkey_buf_len, r_rkey_buf, r_rkey_buf_len, event->payload);
    assert(offload_args_packed_size == packed_size);

    /* rkey_buf is no longer needed */
    ucp_rkey_buffer_release(s_rkey_buf);
    ucp_rkey_buffer_release(r_rkey_buf);

    /* send offload args to DPU */
    rc = event_channel_emit(&event,
                            UCC_TL_UCP_BCAST_HOST_ARRIVE_AM_ID,
                            GET_SERVER_EP(econtext),
                            econtext->client->server_id,
                            NULL);
    if (rc != EVENT_DONE && rc != EVENT_INPROGRESS) {
        tl_error(lib, "event_channel_emit() failed");
        return UCC_ERR_NO_MESSAGE;
    }


    /* deliver local data */
    // int rank = UCC_TL_TEAM_RANK(team);
    // size_t r_displacement = ucc_coll_args_get_displacement(args,
    //                             args->dst.info_v.displacements, rank) *
    //                         ucc_dt_size(args->dst.info_v.datatype);
    size_t r_size = args->dst.info.count * ucc_dt_size(args->dst.info.datatype);
    assert(s_len <= r_size);
    memcpy(r_start, s_start, r_size);


    /* progress collective once */
    status = ucc_tl_ucp_bcast_offload_progress(coll_task);
    if (UCC_INPROGRESS == status) {
        ucc_progress_enqueue(UCC_TL_CORE_CTX(team)->pq, &task->super);
        return UCC_OK;
    }

    return ucc_task_complete(coll_task);
}

ucc_status_t ucc_tl_ucp_bcast_offload_init(ucc_base_coll_args_t *coll_args,
                                                ucc_base_team_t      *team,
                                                ucc_coll_task_t     **task_h)
{
    ucc_tl_ucp_task_t *task = ucc_tl_ucp_init_task(coll_args, team);
    *task_h = &task->super;

    if ((!UCC_DT_IS_PREDEFINED((TASK_ARGS(task)).dst.info.datatype)) ||
        (!UCC_IS_INPLACE(TASK_ARGS(task)) &&
         (!UCC_DT_IS_PREDEFINED((TASK_ARGS(task)).src.info.datatype)))) {
        tl_error(UCC_TASK_LIB(task), "user defined datatype is not supported");
        return UCC_ERR_NOT_SUPPORTED;
    }

    /* initialize active_colls if this is the first bcast coll */
    if (active_colls_initialized == false) {
        ucs_list_head_init(&active_colls);
        active_colls_initialized = true;
    }

    /* allocate a new coll op for tracking */
    bcast_host_coll_t *op = malloc(sizeof(bcast_host_coll_t));
    if (!op) {
        ucs_error("not enough memory");
        return UCS_ERR_NO_MEMORY;
    }

    /* initialize coll op and add it to the active_colls */
    op->coll_task = &task->super;
    op->complete  = 0;
    ucs_list_add_tail(&active_colls, &op->super);

    task->super.post     = ucc_tl_ucp_bcast_offload_start;
    task->super.progress = ucc_tl_ucp_bcast_offload_progress;
    return UCC_OK;
}
           
#endif // HAVE_DPU_OFFLOAD

ucc_status_t ucc_tl_ucp_bcast_init(ucc_tl_ucp_task_t *task)
{
    ucc_tl_ucp_team_t *team      = TASK_TEAM(task);
    ucc_rank_t         team_size = UCC_TL_TEAM_SIZE(team);

    task->bcast_kn.radix =
        ucc_min(UCC_TL_UCP_TEAM_LIB(team)->cfg.bcast_kn_radix, team_size);

    task->super.post     = ucc_tl_ucp_bcast_knomial_start;
    task->super.progress = ucc_tl_ucp_bcast_knomial_progress;
    return UCC_OK;
}

ucc_status_t ucc_tl_ucp_bcast_knomial_init(ucc_base_coll_args_t *coll_args,
                                               ucc_base_team_t *     team,
                                               ucc_coll_task_t **    task_h)
{
    ucc_tl_ucp_task_t *task;
    ucc_status_t       status;

    task    = ucc_tl_ucp_init_task(coll_args, team);
    status  = ucc_tl_ucp_bcast_init(task);
    *task_h = &task->super;
    return status;
}
