/*
 * Generation of MRT files as input for bgpdump2 blaster.
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <arpa/inet.h>

#define WRITEBUFSIZE 1024*256

/*
 * Logging
 */

/*
 * List of log-ids.
 */
enum {
    LOG_ID_MIN,
    NORMAL,
    ERROR,
    BGP,
    IO,
    LOG_ID_MAX
};

struct keyval_ {
    u_int val;       /* value */
    const char *key; /* key */
};

struct __attribute__((__packed__)) log_id_
{
    uint8_t enable;
};

#define LOG(log_id_, fmt_, ...)					\
    do { if (log_id[log_id_].enable) {fprintf(stdout, "%s "fmt_, log_format_timestamp(), ##__VA_ARGS__);} } while (0)

extern struct log_id_ log_id[];
extern char * log_format_timestamp(void);

/*
 * Advertised route.
 */
__attribute__ ((__packed__)) struct rib_entry_ {

    uint32_t seq;
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
    CIRCLEQ_ENTRY(rib_entry_) rib_qnode;
};

typedef struct rib_entry_ rib_entry_t;

/*
 * Top level object.
 */
__attribute__ ((__packed__)) struct ctx_ {

    /* list for walking our routes */
    CIRCLEQ_HEAD(rib_head_, rib_entry_ ) rib_qhead;

    uint32_t num_prefixes; /* To be generated prefixes */
    uint32_t num_nexthops; /* Nexthop limit */

    rib_entry_t base; /* Fill out for all base values */

    /* MRT file */
    char *filename;
    FILE *file;
    int sockfd;

    /* write buffer */
    u_char *write_buf;
    uint write_idx;

    /* epoch */
    time_t now;

    /* MRT peertable */
    uint8_t peer_id[4];
    union {
	uint8_t v4[4];
	uint8_t v6[16];
    } peer_ip;
    uint32_t peer_as;

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

void mrtgen_generate_rib(ctx_t *ctx);
void mrtgen_write_rib(ctx_t *ctx);
void mrtgen_delete_rib(ctx_t *ctx);
