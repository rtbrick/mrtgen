/*
 * Generation of MRT files as input for bgpdump2 blaster.
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

#define OPTIONAL         0x80
#define TRANSITIVE       0x40
#define PARTIAL          0x20
#define EXTENDED_LENGTH  0x10

#define ORIGIN           1
#define AS_PATH          2
#define NEXT_HOP         3
#define MULTI_EXIT_DISC  4
#define LOCAL_PREF       5
#define ATOMIC_AGGREGATE 6
#define AGGREGATOR       7
#define COMMUNITY        8
#define MP_REACH_NLRI    14
#define MP_UNREACH_NLRI  15
#define EXTENDED_COMMUNITY 16
#define LARGE_COMMUNITY  32

#define AS_SEQ 2

#define SAFI_UNICAST       1
#define SAFI_LABEL_UNICAST 4
#define SAFI_VPN_UNICAST 128
