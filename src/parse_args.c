/* ft_ping_args.c ------------------------------------------------------- */
#include "ft_ping.h"

static void usage(int exit_code) {
    fprintf(exit_code ? stderr : stdout,
            "Usage: ft_ping [options] <destination>\n"
            "Options may appear in any order:\n"
            "  -v             verbose output\n"
            "  -q             quiet output (overrides -v)\n"
            "  -c <count>     stop after <count> replies\n"
            "  -i <ms>        interval between pings (milliseconds)\n"
            "  -t <ttl>       set IPv4 TTL / IPv6 hop-limit\n"
            "  -s <bytes>     payload size (default 56)\n"
            "  -?             show this help and exit\n");
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

void parse_args(const int argc, char **argv) {
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
                    fprintf(stderr,
                            "ft_ping: unexpected extra argument '%s'\n",
                            argv[i++]);
                }
                break;
            }

            /* iterate over cluster: e.g. "-vq" becomes 'v' then 'q'    */
            for (size_t j = 1; arg[j]; ++j) {
                int needs_arg;
                if (!lookup_option(arg[j], &needs_arg)) {
                    fprintf(stderr,
                            "ft_ping: unknown option '-%c'\n", arg[j]);
                    usage(1);
                }

                /* ---- flag takes an argument --------------------- */
                if (needs_arg) {
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
                        fprintf(stderr,
                                "ft_ping: option '-%c' "
                                "requires an argument\n", arg[j]);
                        usage(1);
                    }

                    /* validate numeric argument */
                    if (!ft_str_is_number(val)) {
                        fprintf(stderr,
                                "ft_ping: invalid numeric arg '%s' for -%c\n",
                                val, arg[j]);
                        usage(1);
                    }

                    int num = ft_atoi(val);
                    switch (arg[j]) {
                        case 'c': flags.count = num;
                            break;
                        case 'i': flags.interval_ms = num;
                            break;
                        case 't': flags.ttl = num;
                            break;
                        case 'w': flags.timeout = num;
                            break;
                        case 's': flags.payload_size = num;
                            break;
                    }
                }
                /* ---- simple flag, no argument ------------------- */
                else {
                    switch (arg[j]) {
                        case 'v': flags.verbose = 1;
                            break;
                        case 'q': flags.quiet = 1;
                            break;
                        case '?': usage(0);
                            break;
                    }
                }
            }
        }
        /* ---------- positional (target) ----------------------------- */
        else {
            if (target) {
                fprintf(stderr,
                        "ft_ping: multiple destinations: '%s' and '%s'\n",
                        target, arg);
                usage(1);
            }
            target = arg;
        }
    }

    /* ---------- final sanity checks --------------------------------- */
    if (!target) {
        fprintf(stderr, "ft_ping: destination required\n");
        usage(1);
    }
    if (flags.quiet)
        flags.verbose = 0; /* -q overrides -v */
}
