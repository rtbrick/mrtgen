/*
 * Generation of MRT files as input for bgpdump2 blaster mode
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <arpa/inet.h>

#include "mrtgen.h"

/*
 * Globals
 */
int verbose = 1;

/*
 * Prototypes
 */

/*
 * Command line options.
 */
static struct option long_options[] = {
    { "nexthop",            required_argument,  NULL, 'n' },
    { "verbose",            no_argument,        NULL, 'v' },
    { NULL,                 0,                  NULL,  0 }
};

char *
mrtgen_print_usage_arg (struct option *option)
{
    if (option->has_arg == 1) {
        return (char *)" <args>";
    }

    return (char *)"";
}

void
mrtgen_print_usage (void)
{
    u_int idx;

    printf("Usage: mrtgen [OPTIONS]\n\n");

    for (idx = 0; ; idx++) {
        if (long_options[idx].name == NULL) {
            break;
        }

        printf("  -%c --%s%s\n", long_options[idx].val, long_options[idx].name,
               mrtgen_print_usage_arg(&long_options[idx]));
    }
}

void
mrtgen_init_ctx (ctx_t *ctx)
{
    memset(ctx, 0, sizeof(ctx_t));

    ctx->base.as_path[0] = 100000;
    ctx->base.origin = 0; /* IGP */

    /* Prefix */
    ctx->base.prefix_afi = AF_INET;
    ctx->base.prefix_safi = 1; /* unicast */
    inet_pton(AF_INET, "10.0.0.0", &ctx->base.prefix.v4);
    ctx->base.prefix_len = 32;

    /* Nexthop */
    ctx->base.nexthop_afi = AF_INET;
    ctx->base.nexthop_safi = 1; /* unicast */
    inet_pton(AF_INET, "172.16.0.0", &ctx->base.nexthop.v4);

    /* Label */
    ctx->base.label[0] = 100000;
}

int
main (int argc, char *argv[])
{
    int opt, idx;
    ctx_t ctx;

    /*
     * Init default options.
     */
    mrtgen_init_ctx(&ctx);

    /*
     * Parse options.
     */
    idx = 0;
    while ((opt = getopt_long(argc, argv,"c:hv", long_options, &idx )) != -1) {
        switch (opt) {
	case 'n':
	    /* nexthop */
	    if (inet_pton(AF_INET, optarg, &ctx.base.nexthop.v4)) {
		ctx.base.nexthop_afi = AF_INET;
	    } else if (inet_pton(AF_INET6, optarg, &ctx.base.nexthop.v6)) {
		ctx.base.nexthop_afi = AF_INET6;
	    }
	    break;

	case 'v':
	    verbose++;
	    break;
	case 'h': /* fall through */
	default:
	    mrtgen_print_usage();
	    exit(EXIT_FAILURE);
        }
    }

#if 0
    if (1) {
	MRTGEN_LOG("Completed %u SPF repetitions, init %luus, run %luus\n",
		   repeat, init_sum/repeat, run_sum/repeat);
    }
#endif

    /*
     * Flush and close all we have.
     */

    return 0;
}
