/* ft_ping_args.c ------------------------------------------------------- */
#include "ft_ping.h"

static void usage(int exit_code) {
    fprintf(exit_code ? stderr : stdout,
            "Usage: ft_ping [options] <destination>\n"
            "Options may appear in any order:\n"
            "  -v             verbose output\n"
            "  -q             quiet output (overrides -v)\n"
            "  -c <count>     stop after <count> replies\n"
            "  -i <sec>       interval between pings (seconds; supports decimals)\n"
            "  -t <ttl>       set IPv4 TTL / IPv6 hop-limit\n"
            "  -s <bytes>     payload size (default 56)\n"
            "  -?             show this help and exit\n"
    );
    exit(exit_code);
}

/* -------- helpers ---------------------------------------------------- */

/* -------- option table ----------------------------------------------- */
/* One row per single-letter option.  arg_required = 0/1                */
typedef struct s_optdef {
    char letter;
    int arg_required;
} t_optdef;

static const t_optdef g_opts[] = {
    {'v', 0},
    {'q', 0},
    {'c', 1},
    {'i', 1},
    {'t', 1},
    {'w', 1},
    {'s', 1},
    {'?', 0},
    {0, 0}
};

/* Returns 1 if c is recognised option; sets *needs_arg. */
static int lookup_option(char c, int *needs_arg) {
    for (int k = 0; g_opts[k].letter; ++k)
        if (g_opts[k].letter == c) {
            if (needs_arg)
                *needs_arg = g_opts[k].arg_required;
            return (1);
        }
    return (0);
}

/* -------- main parse routine ----------------------------------------- */

void parse_args(int argc, char **argv) {
    // Use global variables directly
    flags = (t_flags){
        .verbose = 0,
        .quiet = 0,
        .count = -1,
        .interval_ms = 1000,
        .ttl = 64,
        .payload_size = 56,
        .timeout = 1000,
    };
    target = NULL;

    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];

        /* ---------- option (starts with '-') ------------------------ */
        if (arg[0] == '-' && arg[1] != '\0') {
            /* handle long “--” as end-of-options marker                */
            if (ft_strcmp(arg, "--") == 0) {
                ++i;
                if (i < argc && !target)
                    target = argv[i++];
                while (i < argc) {
                    /* Not a dedicated error code yet; keep message style consistent. */
                    fprintf(stderr,
                            "ft_ping: unexpected extra argument '%s'\n",
                            argv[i++]
                    );
                    //TODO print help
                }
                break;
            }

            /* iterate over cluster: e.g. "-vq" becomes 'v' then 'q'    */
            for (size_t j = 1; arg[j]; ++j) {
                const char opt = arg[j];
                int needs_arg = 0;
                if (!lookup_option(opt, &needs_arg)) {
                    ping_error_exit(1, PING_ERR_UNKNOWN_OPTION, (int) opt);
                    usage(1);
                }

                /* ---- simple flag, no argument ------------------- */
                if (!needs_arg) {
                    switch (opt) {
                        case 'v': flags.verbose = 1;
                            break;
                        case 'q': flags.quiet = 1;
                            break;
                        case '?': usage(0);
                            break;
                        default:
                            break;
                    }
                    continue;
                }

                /* ---- option takes an argument ------------------- */
                {
                    const char *val = NULL;

                    /* inline: "-c10" or "-c=10" */
                    if (arg[j + 1]) {
                        val = &arg[j + 1];
                        if (*val == '=')
                            ++val;
                        /* end cluster processing after this char     */
                        j = ft_strlen(arg) - 1;
                    }
                    /* separate: "-c" "10" */
                    else if (i + 1 < argc) {
                        val = argv[++i];
                    } else {
                        ping_error_exit(1, PING_ERR_OPTION_REQUIRES_ARG, (int) opt);
                        usage(1);
                    }

                    /* Special handling for -i (interval): enforce minimum. */
                    if (opt == 'i') {
                        if (!ft_str_is_double(val)) {
                            ping_error_exit(2, PING_ERR_INVALID_NUMERIC_ARG, val, (int) opt);
                        }
                        const double seconds = ft_atof(val);
                        /* Reject <= 0 and anything that would round down to 0ms. */
                        if (seconds <= 0.0 || (int) (seconds * 1000.0) < 1) {
                            ping_error_exit(2, PING_ERR_INTERVAL_TOO_SHORT);
                        }
                        flags.interval_ms = (int) (seconds * 1000.0);
                        continue;
                    }

                    /* validate numeric argument */
                    if (!ft_str_is_number(val)) {
                        ping_error_exit(1, PING_ERR_INVALID_NUMERIC_ARG, val, (int) opt);
                        usage(1);
                    }

                    const int num = ft_atoi(val);
                    switch (opt) {
                        case 'c': flags.count = num;
                            break;
                        case 't': flags.ttl = num;
                            break;
                        case 'w': flags.timeout = num;
                            break;
                        case 's': flags.payload_size = num;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        /* ---------- positional (target) ----------------------------- */
        else {
            if (target) {
                ping_error_exit(1, PING_ERR_MULTIPLE_DESTINATIONS, target, arg);
                usage(1);
            }
            target = arg;
        }
    }

    /* ---------- final sanity checks --------------------------------- */
    if (!target) {
        ping_error_exit(1, PING_ERR_DESTINATION_REQUIRED);
        usage(1);
    }
    if (flags.quiet)
        flags.verbose = 0; /* -q overrides -v */
}
