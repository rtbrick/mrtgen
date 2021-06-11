/*
 * Generation of MRT files as input for bgpdump2 blaster mode
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

#include "mrtgen.h"

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

/*
 * Generate the RIB that we're about to write.
 */
void
mrtgen_generate_rib (ctx_t *ctx)
{
    rib_entry_t *re;
    rib_entry_t re_templ;
    __uint128_t addr, prefix_inc;
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

    for (seq = 0; seq < ctx->num_routes; seq++) {
	re = malloc(sizeof(rib_entry_t));
	if (!re) {
	    LOG(ERROR, "Could not allocate rib-entry\n");
	    return;
	}

	/*
	 * Copy params from template.
	 */
	memcpy(re, &re_templ, sizeof(rib_entry_t));

	/*
	 * Increment iterators in template.
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
