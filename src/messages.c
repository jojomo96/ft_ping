#include "ft_ping.h"


/*
** Message Table
** The order MUST match the t_msg_id enum.
*/
static const char *g_msg_table[] = {
    [MSG_USAGE_TITLE] = "Usage: ft_ping [options] <destination>",
    [MSG_ERR_DEST_REQ] = "destination required",
    [MSG_ERR_MULTIPLE_DEST] = "multiple destinations provided: '%s'",
    [MSG_ERR_UNEXPECTED_ARG] = "unexpected argument: '%s'",
    [MSG_ERR_UNKNOWN_OPT] = "unknown option -- '%c'",
    [MSG_ERR_UNRECOG_OPT] = "unrecognized option '%s'",
    [MSG_ERR_OPT_REQ_ARG] = "option requires an argument: '%s'",
    [MSG_ERR_INVALID_NUM] = "invalid value '%s' for option",
    [MSG_ERR_TTL_RANGE] = "ttl out of range: %s",

    [MSG_ERR_INVALID_COUNT] = "invalid count: '%s'",
    [MSG_ERR_COUNT_RANGE] = "count must be >= 1: '%s'",

    [MSG_ERR_INVALID_TTL] = "invalid TTL: '%s'",

    [MSG_ERR_INVALID_SIZE] = "invalid packet size: '%s'",
    [MSG_ERR_SIZE_RANGE] = "size cannot be negative: '%s'",

    [MSG_ERR_INVALID_TIMEOUT] = "invalid timeout: '%s'",

    [MSG_ERR_INVALID_INTERVAL] = "invalid interval: '%s'",
    [MSG_ERR_INTERVAL_SHORT] = "interval too short: '%s'",

    [MSG_ERR_INVALID_TYPE] = "invalid type: '%s'",
};

/*
** Internal helper to print the formatted message.
** Adds "ft_ping: " prefix automatically.
*/
static void print_formatted(FILE *stream, t_msg_id id, va_list args) {
    if (id < 0 || id >= (sizeof(g_msg_table) / sizeof(char *)))
        return;

    fprintf(stream, "ft_ping: ");
    vfprintf(stream, g_msg_table[id], args);
    fprintf(stream, "\n");
}

/*
** Prints a message to stderr.
** Usage: ping_msg(MSG_ERR_UNKNOWN_OPT, 'c');
*/
void ping_msg(t_msg_id id, ...) {
    va_list args;

    va_start(args, id);
    print_formatted(stderr, id, args);
    va_end(args);
}

/*
** Prints a message to stderr and exits with status 1.
** Usage: ping_fatal(MSG_ERR_DEST_REQ);
*/
void ping_fatal(t_msg_id id, ...) {
    va_list args;

    va_start(args, id);
    print_formatted(stderr, id, args);
    va_end(args);
    exit(1);
}

void ft_usage(const int exit_code) {
    fprintf(stdout, "Usage: ft_ping [options] <destination>\n\nOptions:\n");
    for (int i = 0; g_options[i].handler; i++) {
        char buf[64];
        int len = 0;
        const t_ping_opt *opt = &g_options[i];

        /* Format short opt */
        if (opt->short_name)
            len += sprintf(buf + len, "  -%c", opt->short_name);
        else
            len += sprintf(buf + len, "    ");

        /* Format separator */
        len += sprintf(buf + len, "%s", (opt->short_name && opt->long_name) ? ", " : "  ");

        /* Format long opt */
        if (opt->long_name)
            len += sprintf(buf + len, "--%s", opt->long_name);

        /* Format argument label */
        if (opt->type == ARG_REQ && opt->arg_label)
            len += sprintf(buf + len, " <%s>", opt->arg_label);

        fprintf(stdout, "%-35s %s\n", buf, opt->desc);
    }
    exit(exit_code);
}
