/*
 * Generation of MRT files as input for bgpdump2 blaster.
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

/* type */
#define MRT_TABLE_DUMP_V2 13

/* subtype */
#define MRT_PEER_INDEX_TABLE 1
#define MRT_RIB_IPV4_UNICAST 2
#define MRT_RIB_IPV6_UNICAST 4
#define MRT_RIB_GENERIC      6

#define MRT_PEER_TYPE_AS4  0x2
#define MRT_PEER_TYPE_IPV6 0x1
