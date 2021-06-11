/*
 * Generation of MRT files as input for bgpdump2 blaster.
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

/*
 * Advertised route.
 */
__attribute__ ((__packed__)) struct rib_entry_ {

    uint32_t as_path[8]; /* Assume AS Path won't be larger than 8 */
    uint8_t origin;

    /* Prefix */
    uint8_t prefix_afi;
    uint8_t prefix_safi;
    uint8_t prefix_len;
    union {
	uint8_t v4[4];
	uint8_t v6[16];
    } prefix;

    /* Nexthop */
    uint8_t nexthop_afi;
    uint8_t nexthop_safi;
    union {
	uint8_t v4[4];
	uint8_t v6[16];
    } nexthop;

    uint32_t label[4];

    /* double linked list to conveniently walk */
    CIRCLEQ_ENTRY(rib_entry_) rib_entry_qnode;
};

typedef struct rib_entry_ rib_entry_t;

/*
 * Top level object.
 */
__attribute__ ((__packed__)) struct ctx_ {

    /* list for walking our routes */
    CIRCLEQ_HEAD(rib_head_, rib_entry_ ) rib_qhead;

    uint32_t num_routes; /* To be generated routes */
    rib_entry_t base; /* Fill out for all base values */

    /* scratchpad */
    struct {
	struct sockaddr_in  addr4;
	struct sockaddr_in6 addr6;
	uint8_t prefix_len;
    } sp;
};
typedef struct ctx_ ctx_t;

/*
 * External API
 */

