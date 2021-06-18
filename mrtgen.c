/*
 * Generation of MRT files as input for bgpdump2 blaster mode
 *
 * Hannes Gredler, June 2021
 *
 * Copyright (C) 2015-2021, RtBrick, Inc.
 */

#include "mrtgen.h"

/*
 * Globals
 */
int verbose = 1;
struct log_id_ log_id[LOG_ID_MAX];

/*
 * Prototypes
 */

const char banner[] = "\n"
"      ____   __   ____         _        __            ,/\n"
"     / __ \\ / /_ / __ ) _____ (_)_____ / /__        ,'/\n"
"    / /_/ // __// __  |/ ___// // ___// //_/      ,' /\n"
"   / _, _// /_ / /_/ // /   / // /__ / ,<       ,'  /_____,    \n"
"  /_/ |_| \\__//_____//_/   /_/ \\___//_/|_|    .'____    ,'   \n"
"      __  _______  ____________                    /  ,'\n"
"     /  |/  / __ \\/_  __/ ____/_   ____           / ,'\n"
"    / /|_/ / /_/ / / / / / __/ _ \\/ __ \\         /,'\n"
"   / /  / / _, _/ / / / /_/ /  __/ / / /        / \n"
"  /_/  /_/_/ |_| /_/  \\____/\\___/_/ /_/\n\n";

/*
 * Log target / name translation table.
 */
struct keyval_ log_names[] = {
    { BGP,           "bgp" },
    { IO,            "io" },
    { ERROR,         "error" },
    { NORMAL,        "normal" },
    { 0, NULL}
};

const char *
keyval_get_key (struct keyval_ *keyval, int val)
{
    struct keyval_ *ptr;

    ptr = keyval;
    while (ptr->key) {
	if (ptr->val == val) {
	    return ptr->key;
	}
	ptr++;
    }
    return "unknown";
}

/*
 * Format the logging timestamp.
 */
char *
log_format_timestamp (void)
{
    static char ts_str[sizeof("Dec 24 08:07:13.711541")];
    struct timespec now;
    struct tm tm;
    int len;

    clock_gettime(CLOCK_REALTIME, &now);
    localtime_r(&now.tv_sec, &tm);

    len = strftime(ts_str, sizeof(ts_str), "%b %d %H:%M:%S", &tm);
    snprintf(ts_str+len, sizeof(ts_str) - len, ".%06lu",
	     now.tv_nsec / 1000);

    return ts_str;
}

/*
 * Enable logging.
 */
void
log_enable (char *log_name)
{
    int idx;

    idx = 0;
    while (log_names[idx].key) {
	if (strcmp(log_names[idx].key, log_name) == 0) {
	    log_id[log_names[idx].val].enable = 1;
	}
	idx++;
    }
}

/*
 * Format the prefix of the rib-entry.
 */
char *
format_prefix (rib_entry_t *re)
{
    static char buf[128];
    static char plen_buf[8];
    size_t len;

    switch (re->prefix_afi) {
    case AF_INET:
	inet_ntop(AF_INET, &re->prefix.v4, buf, sizeof(buf));
	break;
    case AF_INET6:
	inet_ntop(AF_INET6, &re->prefix.v6, buf, sizeof(buf));
	break;
    default:
	snprintf(buf, sizeof(buf), "unknown afi %u", re->prefix_afi);
    }

    snprintf(plen_buf, sizeof(plen_buf), "/%u", re->prefix_len);
    strncat(buf, plen_buf, sizeof(buf));

    return buf;
}

/*
 * Format the nexthop of the rib-entry.
 */
char *
format_nexthop (rib_entry_t *re)
{
    static char buf[128];

    switch (re->nexthop_afi) {
    case AF_INET:
	inet_ntop(AF_INET, &re->nexthop.v4, buf, sizeof(buf));
	break;
    case AF_INET6:
	inet_ntop(AF_INET6, &re->nexthop.v6, buf, sizeof(buf));
	break;
    default:
	snprintf(buf, sizeof(buf), "unknown afi %u", re->nexthop_afi);
    }

    return buf;
}

/*
 * Command line options.
 */
static struct option long_options[] = {
    { "as-base",            required_argument,  NULL, 'a' },
    { "log",                required_argument,  NULL, 't' },
    { "local-preference",   required_argument,  NULL, 'l' },
    { "label-base",         required_argument,  NULL, 'm' },
    { "nexthop-base",       required_argument,  NULL, 'n' },
    { "nexthop-num",        required_argument,  NULL, 'N' },
    { "prefix-base",        required_argument,  NULL, 'p' },
    { "prefix-num",         required_argument,  NULL, 'P' },
    { "verbose",            no_argument,        NULL, 'v' },
    { NULL,                 0,                  NULL,  0 }
};

char *
mrtgen_print_usage_arg (struct option *option)
{
    static char buf[128];
    struct keyval_ *ptr;
    int len;

    if (option->has_arg == 1) {

	if (strcmp(option->name, "logging") == 0) {
	    len = 0;
	    ptr = log_names;
	    while (ptr->key) {
		len += snprintf(buf+len, sizeof(buf)-len, "%s%s", len ? "|" : " ", ptr->key);
		ptr++;
	    }
	    return buf;
	}

	return " <args>";
    }
    return "";
}

void
mrtgen_print_usage (void)
{
    u_int idx;

    printf("%s", banner);
    printf("Usage: mrtgen [OPTIONS]\n\n");

    for (idx = 0; ; idx++) {
        if (long_options[idx].name == NULL) {
            break;
        }

        printf("  -%c --%s%s\n", long_options[idx].val, long_options[idx].name,
               mrtgen_print_usage_arg(&long_options[idx]));
    }
}

/*
 * BGP origin type codes
 */
struct keyval_ bgp_origin_types[] = {
    { 0,       "IGP" },
    { 1,       "EGP" },
    { 2,       "Incomplete" },
    { 0, NULL}
};


void
mrtgen_init_ctx (ctx_t *ctx)
{
    memset(ctx, 0, sizeof(ctx_t));

    CIRCLEQ_INIT(&ctx->rib_qhead);

    ctx->filename = "gen.mrt"; /* Filename */

    //    ctx->num_prefixes = 50000; /* Number of prefixes */
    ctx->num_prefixes = 10; /* Number of prefixes */
    ctx->num_nexthops = 2000; /* Number of nexthops */

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

    /* Write buffer */
    ctx->write_buf = malloc(WRITEBUFSIZE);

    /* MRT must haves */
    time(&ctx->now);
    inet_pton(AF_INET, "192.168.1.1", &ctx->peer_id);
    inet_pton(AF_INET, "192.168.1.1", &ctx->peer_ip.v4);
    ctx->peer_as = 4200000000;
}

void
mrtgen_log_ctx (ctx_t *ctx)
{
    LOG(NORMAL, "MRT prefix generation parameters for file %s\n", ctx->filename);
    LOG(NORMAL, " Origin %s\n", keyval_get_key(bgp_origin_types, ctx->base.origin));
    LOG(NORMAL, " Base AS %u\n", ctx->base.as_path[0]);
    LOG(NORMAL, " Base Prefix %s, %u prefixes\n", format_prefix(&ctx->base), ctx->num_prefixes);
    LOG(NORMAL, " Base Nexthop %s, %u nexthops\n", format_nexthop(&ctx->base), ctx->num_nexthops);
    if (ctx->base.label[0]) {
	LOG(NORMAL, " Base label %u\n", ctx->base.label[0]);
    }
    if (ctx->base.localpref) {
	LOG(NORMAL, " Local preference %u\n", ctx->base.localpref);
    }
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
    log_id[NORMAL].enable = true;
    log_id[ERROR].enable = true;

    /*
     * Parse options.
     */
    idx = 0;
    while ((opt = getopt_long(argc, argv,"a:t:l:m:n:N:p:P:hv", long_options, &idx )) != -1) {
        switch (opt) {
        case 't':
	    /* logging */
	    log_enable(optarg);
	    break;

	case 'a':
	    /* base AS */
	    ctx.base.as_path[0] = atoi(optarg);
	    break;

	case 'm':
	    /* base label */
	    ctx.base.label[0] = atoi(optarg);
	    break;

	case 'l':
	    /* localpref */
	    ctx.base.localpref = atoi(optarg);
	    break;

        case 'P':
	    /* number of prefixes */
	    ctx.num_prefixes = atoi(optarg);
	    break;

	case 'p':
	    /* base prefix */
	    {
		char *tok;

		tok = strtok(optarg, "/");
		if (tok && inet_pton(AF_INET, tok, &ctx.base.prefix.v4)) {
		    ctx.base.prefix_afi = AF_INET;
		} else if (tok && inet_pton(AF_INET6, tok, &ctx.base.prefix.v6)) {
		    ctx.base.prefix_afi = AF_INET6;
		}
		tok = strtok(NULL, "/");
		if (tok) {
		    ctx.base.prefix_len = atoi(tok);
		}
	    }
	    break;

	case 'N':
	    /* number of nexthops */
	    ctx.num_nexthops = atoi(optarg);
	    break;

	case 'n':
	    /* base nexthop */
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

    printf("%s", banner);

    /*
     * Log configured options
     */
    mrtgen_log_ctx(&ctx);

    /*
     * Generate RIB
     */
    mrtgen_generate_rib(&ctx);

    /*
     * Open file
     */
    ctx.file = fopen(ctx.filename, "w");
    if (!ctx.file) {
	LOG(ERROR, "Could not open MRT file %s", ctx.filename);
	return 0;
    }
    ctx.sockfd = fileno(ctx.file);
    if (ctx.sockfd == -1) {
	LOG(ERROR, "Could not set FD for MRT file %s", ctx.filename);
	return 0;
    }

    /*
     * Write RIB
     */
    mrtgen_write_rib(&ctx);

    /*
     * Flush and close all we have.
     */
    mrtgen_delete_rib(&ctx);
    free(ctx.write_buf);
    fclose(ctx.file);

    return 0;
}
