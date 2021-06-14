/*
 * Generation of MRT files as input for bgpdump2 blaster mode
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

#include "mrtgen.h"
#include "mrt.h"
#include "bgp.h"

__uint128_t
mrtgen_load_addr (uint8_t *buf, uint len)
{
    __uint128_t addr;
    uint idx;

    addr = *buf;
    for (idx = 1; idx < len; idx++) {
	addr = addr << 8;
	addr |= buf[idx];
    }

    return addr;
}

void
mrtgen_store_addr (__uint128_t addr, uint8_t *buf, uint len)
{
    uint idx;

    for (idx = len; idx; idx--) {
	buf[idx-1] = addr & 0xff;
	addr = addr >> 8;
    }
}

void
mrtgen_copy_addr (uint8_t *dst, uint8_t *src, uint len)
{
    __uint128_t addr;

    addr = mrtgen_load_addr(src, len);
    mrtgen_store_addr(addr, dst, len);
}

/*
 * Generate the RIB that we're about to write.
 */
void
mrtgen_generate_rib (ctx_t *ctx)
{
    rib_entry_t *re;
    rib_entry_t re_templ;
    __uint128_t addr, prefix_inc;
    __uint128_t nexthop_inc = 1;
    uint seq;

    switch(ctx->base.prefix_afi) {
    case AF_INET:
	prefix_inc = 1 << (32 - ctx->base.prefix_len);
	break;
    case AF_INET6:
	prefix_inc = 1 << (128 - ctx->base.prefix_len);
	break;
    default:
	prefix_inc = 0;
    }

    /*
     * Copy the base to the template.
     */
    memcpy(&re_templ, &ctx->base, sizeof(rib_entry_t));

    for (seq = 0; seq < ctx->num_prefixes; seq++) {
	re = malloc(sizeof(rib_entry_t));
	if (!re) {
	    LOG(ERROR, "Could not allocate rib-entry\n");
	    return;
	}

	/*
	 * Copy params from template.
	 */
	re_templ.seq = seq;
	memcpy(re, &re_templ, sizeof(rib_entry_t));

	/*
	 * Increment prefix in template.
	 */
	switch (re_templ.prefix_afi) {
	case AF_INET:
	    addr = mrtgen_load_addr(re_templ.prefix.v4, 4);
	    addr += prefix_inc;
	    mrtgen_store_addr(addr, re_templ.prefix.v4, 4);
	    break;
	case AF_INET6:
	    addr = mrtgen_load_addr(re_templ.prefix.v6, 16);
	    addr += prefix_inc;
	    mrtgen_store_addr(addr, re_templ.prefix.v6, 16);
	    break;
	}

	/*
	 * Increment nexthop in template.
	 */
	switch (re_templ.nexthop_afi) {
	case AF_INET:
	    addr = mrtgen_load_addr(re_templ.nexthop.v4, 4);
	    addr += nexthop_inc;
	    mrtgen_store_addr(addr, re_templ.nexthop.v4, 4);
	    break;
	case AF_INET6:
	    addr = mrtgen_load_addr(re_templ.nexthop.v6, 16);
	    addr += nexthop_inc;
	    mrtgen_store_addr(addr, re_templ.nexthop.v6, 16);
	    break;
	}

	/*
	 * Add to list.
	 */
	CIRCLEQ_INSERT_TAIL(&ctx->rib_qhead, re, rib_qnode);
    }
}

/*
 * Flush the entire rib.
 */
void
mrtgen_delete_rib (ctx_t *ctx)
{
    rib_entry_t *re;

    while (!CIRCLEQ_EMPTY(&ctx->rib_qhead)) {
	re = CIRCLEQ_FIRST(&ctx->rib_qhead);
	CIRCLEQ_REMOVE(&ctx->rib_qhead, re, rib_qnode);
	free(re);
    }
}

/*
 * Flush the write buffer.
 * return 0 if buffer is empty and if the buffer has been fully drained.
 * return 1 if there is still some data lurking in the buffer.
 */
int
mrtgen_fflush (ctx_t *ctx)
{
    int res;

    if (!ctx->write_idx) {
        return 0;
    }

    res = write(ctx->sockfd, ctx->write_buf, ctx->write_idx);

    /*
     * Blocked ?
     */
    if (res == -1) {
        switch (errno) {
        case EAGAIN:
	    break;

	case EPIPE:
	    return 0;

        default:
	    LOG(ERROR, "write(): error %s (%d)\n", strerror(errno), errno);
	    break;
        }
	return 1;
    }

    /*
     * Full write ?
     */
    if (res == (int)ctx->write_idx) {
	LOG(IO, "Full write %u bytes buffer to %s\n", res, ctx->filename);
	ctx->write_idx = 0;
	return 0;
    }

    /*
     * Partial write ?
     */
    if (res && res < (int)ctx->write_idx) {
	LOG(IO, "Partial write %u bytes buffer to %s\n", res, ctx->filename);

	/*
	 * Rebase the buffer.
	 */
	memmove(ctx->write_buf, ctx->write_buf+res, ctx->write_idx - res);
	ctx->write_idx -= res;
	return 1;
    }

    return 0;
}

/*
 * Quick'n dirty big endian writer.
 */
void
write_be_uint (u_char *data, uint length, unsigned long long value)
{
    uint idx;

    if (!length || length > 8) {
	return;
    }

    for (idx = 0; idx < length; idx++) {
	data[length - idx -1] =  value & 0xff;
	value >>= 8;
    }
}

/*
 * Push data to the write buffer and update the cursor.
 */
void
push_be_uint (ctx_t *ctx, uint length, unsigned long long value)
{
    /*
     * Buffer 90% full ?
     */
    if ((ctx->write_idx) >= ((WRITEBUFSIZE*9)/10)) {
	mrtgen_fflush(ctx);
    }

    /*
     * Write the data.
     */
    write_be_uint(ctx->write_buf + ctx->write_idx, length, value);

    /*
     * Adjust the cursor.
     */
    ctx->write_idx += length;
}

void
mrtgen_write_peertable (ctx_t *ctx)
{
    __uint128_t addr;
    uint start_idx, length_idx, length;

    start_idx = ctx->write_idx;

    push_be_uint(ctx, 4, ctx->now); /* timestamp */
    push_be_uint(ctx, 2, MRT_TABLE_DUMP_V2); /* type */
    push_be_uint(ctx, 2, MRT_PEER_INDEX_TABLE); /* subtype */

    push_be_uint(ctx, 4, 0); /* length */
    length_idx = ctx->write_idx;

    push_be_uint(ctx, 4, 0x12345678); /* collector id */
    push_be_uint(ctx, 2, 0); /* view name length */
    push_be_uint(ctx, 2, 1); /* peer count */

    switch (ctx->base.prefix_afi) {
    case AF_INET:
	push_be_uint(ctx, 1, 0x2); /* peer type ipv4, 32-bit AS */
	addr = mrtgen_load_addr(ctx->peer_id, 4);
	push_be_uint(ctx, 4, addr); /* peer bgp-id */

	addr = mrtgen_load_addr(ctx->peer_ip.v4, 4);
	push_be_uint(ctx, 4, addr); /* peer ipv4 */

	push_be_uint(ctx, 4, ctx->peer_as); /* peer as */
	break;
    case AF_INET6:
	push_be_uint(ctx, 1, 0x3); /* peer type ipv6, 32-bit AS */
	addr = mrtgen_load_addr(ctx->peer_id, 4);
	push_be_uint(ctx, 4, addr); /* peer bgp-id */

	addr = mrtgen_load_addr(ctx->peer_ip.v6, 16); /* peer ipv6 */
	mrtgen_store_addr(addr, ctx->write_buf+ctx->write_idx, 16);
	ctx->write_buf += 16;

	push_be_uint(ctx, 4, ctx->peer_as); /* peer as */
	break;
    }

    length = ctx->write_idx - length_idx;
    write_be_uint(ctx->write_buf+start_idx+8, 4, length);
}

uint
mrtgen_get_rib_subtype (rib_entry_t *re)
{
    uint32_t af;

    af = re->prefix_afi << 8 | re->prefix_safi;
    switch (af) {
    case (AF_INET << 8 | 1):
	return MRT_RIB_IPV4_UNICAST;
    case (AF_INET6 << 8 | 1):
	return MRT_RIB_IPV6_UNICAST;
    default:
	return MRT_RIB_GENERIC;
    }
}

void
mrtgen_write_pa (ctx_t *ctx, rib_entry_t *re)
{
    uint8_t pa_flags;
    uint as_path_idx, as_path_length;
    uint idx, seg_len;

    /* Origin */
    pa_flags = 0;
    push_be_uint(ctx, 1, pa_flags); /* flags */
    push_be_uint(ctx, 1, ORIGIN); /* type */
    push_be_uint(ctx, 1, 1); /* length */
    push_be_uint(ctx, 1, re->origin);

    /* AS PATH */
    push_be_uint(ctx, 1, pa_flags); /* flags */
    push_be_uint(ctx, 1, AS_PATH); /* type */
    push_be_uint(ctx, 1, 0); /* length */
    as_path_idx = ctx->write_idx;

    /* count seg_len */
    for (seg_len = 0; seg_len < AS_PATH_MAX; seg_len++) {
	if (!re->as_path[seg_len]) {
	    break;
	}
    }
    push_be_uint(ctx, 1, AS_SEQ); /* path segment type */
    push_be_uint(ctx, 1, seg_len); /* seg_len */
    for (idx = 0; idx < seg_len; idx++) {
	push_be_uint(ctx, 4, re->as_path[idx]);
    }
    as_path_length = ctx->write_idx - as_path_idx;
    write_be_uint(ctx->write_buf+as_path_idx-1, 1, as_path_length); /* Update AS Path length field */

    /* IPv4 nexthop */
    if (re->nexthop_afi == AF_INET) {
	push_be_uint(ctx, 1, pa_flags); /* flags */
	push_be_uint(ctx, 1, NEXT_HOP); /* type */
	push_be_uint(ctx, 1, 4); /* length */
	mrtgen_copy_addr(ctx->write_buf+ctx->write_idx, re->nexthop.v4, 4);
	ctx->write_idx += 4;
    }

    /* Local Pref */
    if (re->localpref) {
	push_be_uint(ctx, 1, pa_flags); /* flags */
	push_be_uint(ctx, 1, LOCAL_PREF); /* type */
	push_be_uint(ctx, 1, 4); /* length */
	push_be_uint(ctx, 4, re->localpref);
    }
}

void
mrtgen_write_ribentry (ctx_t *ctx, rib_entry_t *re)
{
    __uint128_t addr;
    uint start_idx, length_idx, length, pa_length_idx, pa_length;

    start_idx = ctx->write_idx;

    push_be_uint(ctx, 4, ctx->now); /* timestamp */
    push_be_uint(ctx, 2, MRT_TABLE_DUMP_V2); /* type */
    push_be_uint(ctx, 2, mrtgen_get_rib_subtype(re)); /* subtype */

    push_be_uint(ctx, 4, 0); /* length */
    length_idx = ctx->write_idx;

    push_be_uint(ctx, 4, re->seq); /* sequence */
    push_be_uint(ctx, 1, re->prefix_len); /* prefix length */
    switch (re->prefix_afi) {
    case AF_INET:
	mrtgen_copy_addr(ctx->write_buf+ctx->write_idx, re->prefix.v4, 4);
	break;
    case AF_INET6:
	mrtgen_copy_addr(ctx->write_buf+ctx->write_idx, re->prefix.v6, 16);
	break;
    }
    ctx->write_idx += (re->prefix_len + 7) / 8; /* packed prefix encoding */
    push_be_uint(ctx, 2, 1); /* entry count */

    push_be_uint(ctx, 2, 0); /* peer_index */
    push_be_uint(ctx, 4, ctx->now); /* originated timestamp */

    push_be_uint(ctx, 2, 0); /* BGP path attribute length */
    pa_length_idx = ctx->write_idx;
    mrtgen_write_pa(ctx, re);
    pa_length = ctx->write_idx - pa_length_idx;
    write_be_uint(ctx->write_buf+pa_length_idx-2, 2, pa_length); /* Update PA length field */

    length = ctx->write_idx - length_idx;
    write_be_uint(ctx->write_buf+start_idx+8, 4, length); /* Update length field */
}

/*
 * Write the entire RIB into a MRT file.
 * Use a buffered write for this.
 */
void
mrtgen_write_rib (ctx_t *ctx)
{
    rib_entry_t *re;
    uint count;

    /*
     * First write the peer table.
     */
    mrtgen_write_peertable(ctx);

    /*
     * Next write a set of RIB entries.
     */
    count = 0;
    CIRCLEQ_FOREACH(re, &ctx->rib_qhead, rib_qnode) {
	mrtgen_write_ribentry(ctx, re);
	count++;
    }

    mrtgen_fflush(ctx);
    LOG(NORMAL, "Wrote %u rib-entries to %s\n", count, ctx->filename);
}
