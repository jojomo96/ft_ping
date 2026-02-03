#include "ft_ping.h"
#include "ft_messages.h"
#include "libft/libft.h"


static int match_long(const char *arg, const t_ping_opt *opt, char **val_out) {
    if (!opt->long_name) return (0);
    /* Strict double-dash enforcement */
    if (ft_strncmp(arg, "--", 2) != 0) return (0);

    const char *name = arg + 2;
    const size_t len = ft_strlen(opt->long_name);

    if (ft_strncmp(name, opt->long_name, len) != 0) return (0);

    const char suffix = name[len];
    if (suffix == '\0') {
        *val_out = NULL;
        return (1);
    }
    if (suffix == '=') {
        *val_out = (char *) (name + len + 1);
        return (1);
    }

    /* Support attached style (--ttl99) if option expects argument */
    if (opt->type == ARG_REQ && ft_isdigit(suffix)) {
        *val_out = (char *) (name + len);
        return (1);
    }
    return (0);
}

/* --- Main Engine --- */

void parse_args(int argc, char **argv) {
    flags = (t_flags){.interval_ms = 1000, .ttl = 64, .payload_size = 56, .timeout = 1000, .count = -1};
    target = NULL;

    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];

        /* 1. Positional */
        if (arg[0] != '-' || arg[1] == '\0') {
            if (target) {
                ping_fatal(MSG_ERR_MULTIPLE_DEST, arg);
            }
            target = arg;
            continue;
        }

        /* 2. Terminator */
        if (ft_strcmp(arg, "--") == 0) {
            i++;
            if (i < argc && !target) target = argv[i++];
            if (i < argc) {
                ping_fatal(MSG_ERR_UNEXPECTED_ARG, argv[i]);
            }
            break;
        }

        const t_ping_opt *match = NULL;
        char *val = NULL;

        /* A. Try Long Options */
        for (int k = 0; g_options[k].handler; k++) {
            if (match_long(arg, &g_options[k], &val)) {
                match = &g_options[k];
                break;
            }
        }

        /* B. Try Short Options */
        if (!match) {
            /* Check if it looked like a long option (begins with --) */
            if (arg[1] == '-') {
                ping_msg(MSG_ERR_UNRECOG_OPT, arg);
                ft_usage(1);
            }

            for (size_t j = 1; arg[j]; j++) {
                const t_ping_opt *short_match = NULL;
                for (int k = 0; g_options[k].handler; k++) {
                    if (g_options[k].short_name == arg[j]) {
                        short_match = &g_options[k];
                        break;
                    }
                }

                if (!short_match) {
                    ping_msg(MSG_ERR_UNKNOWN_OPT, arg[j]);
                    ft_usage(1);
                }

                if (short_match->type == ARG_REQ) {
                    if (arg[j + 1]) val = &arg[j + 1];
                    else if (i + 1 < argc) val = argv[++i];
                    else ping_fatal(MSG_ERR_OPT_REQ_ARG, "NULL"); /* Generic error for missing arg */

                    short_match->handler(val);
                    break;
                } else {
                    short_match->handler(NULL);
                    if (short_match->handler == handle_help) ft_usage(0);
                }
            }
            continue;
        }

        /* C. Execute Long Match */
        if (match) {
            if (match->type == ARG_REQ) {
                if (!val) {
                    if (i + 1 < argc) val = argv[++i];
                    else ping_fatal(MSG_ERR_OPT_REQ_ARG, match->long_name);
                }
                match->handler(val);
            } else {
                match->handler(NULL);
                if (match->handler == handle_help) ft_usage(0);
            }
        }
    }

    if (!target) {
        ping_msg(MSG_ERR_DEST_REQ);
        ft_usage(1);
    }
    if (flags.quiet) flags.verbose = 0;
}
