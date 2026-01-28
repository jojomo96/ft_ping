#include "ft_ping.h"
#include "ft_messages.h"
#include <math.h>
#include <stdio.h>


/*
\*\* Message Table
\*\* The order MUST match the t\_msg\_id enum.
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

    /* runtime/info */
    [MSG_ERR_UNKNOWN_HOST] = "unknown host: %s",
    [MSG_ERR_SOCKET] = "socket: %s",
    [MSG_ERR_SENDTO] = "sendto: %s",
    [MSG_ERR_RECVMSG] = "recvmsg: %s",
    [MSG_ERR_SETSOCKOPT_TIMEOUT] = "setsockopt(SO_RCVTIMEO): %s",
    [MSG_ERR_SETSOCKOPT_TTL] = "setsockopt(IP_TTL): %s",

    [MSG_PING_HEADER] = "PING %s (%s): %d data bytes",
    [MSG_PING_REPLY] = "%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms",
    [MSG_PING_FROM] = "From %s: icmp_seq=%d %s",

    [MSG_STATS_HEADER] = "--- %s ping statistics ---",
    [MSG_STATS_SUMMARY] = "%ld packets transmitted, %ld received, %.0f%% packet loss, time %.0fms",
    [MSG_STATS_RTT] = "rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms",

    [MSG_USAGE_OPTIONS_HEADER] = "Options:",
    [MSG_USAGE_OPTION_LINE] = "%-35s %s",
};

static void print_formatted(FILE *stream, t_msg_id id, va_list args) {
    const size_t n = sizeof(g_msg_table) / sizeof(g_msg_table[0]);
    if ((size_t)id >= n || !g_msg_table[id])
        return;

    fprintf(stream, "ft_ping: ");
    vfprintf(stream, g_msg_table[id], args);
    fprintf(stream, "\n");
}

void ping_msg(t_msg_id id, ...) {
    va_list args;

    va_start(args, id);
    print_formatted(stderr, id, args);
    va_end(args);
}

void ping_fatal(t_msg_id id, ...) {
    va_list args;

    va_start(args, id);
    print_formatted(stderr, id, args);
    va_end(args);
    exit(1);
}

void ft_usage(const int exit_code) {
    /* Keep everything routed through messages.c */
    ping_msg(MSG_USAGE_TITLE);
    ping_msg(MSG_USAGE_OPTIONS_HEADER);

    for (int i = 0; g_options[i].handler; i++) {
        char buf[64];
        int len = 0;
        const t_ping_opt *opt = &g_options[i];

        if (opt->short_name)
            len += sprintf(buf + len, "  -%c", opt->short_name);
        else
            len += sprintf(buf + len, "    ");

        len += sprintf(buf + len, "%s", (opt->short_name && opt->long_name) ? ", " : "  ");

        if (opt->long_name)
            len += sprintf(buf + len, "--%s", opt->long_name);

        if (opt->type == ARG_REQ && opt->arg_label)
            len += sprintf(buf + len, " <%s>", opt->arg_label);

        ping_msg(MSG_USAGE_OPTION_LINE, buf, opt->desc);
    }
    exit(exit_code);
}

void print_stats(const t_stats *stats) {
    if (!stats)
        return;

    const double total = get_time_ms() -
                         ((stats->start_tv.tv_sec * 1000.0) + (stats->start_tv.tv_usec / 1000.0));
    double loss = 0.0;

    if (stats->tx > 0)
        loss = ((stats->tx - stats->rx) * 100.0) / stats->tx;

    /* Header on its own line (leading newline without printf) */
    ping_msg(MSG_STATS_HEADER, target);
    ping_msg(MSG_STATS_SUMMARY, stats->tx, stats->rx, loss, total);

    if (stats->rx > 0) {
        const double avg = stats->sum / stats->rx;
        const double mdev = sqrt((stats->sq_sum / stats->rx) - (avg * avg));
        ping_msg(MSG_STATS_RTT, stats->min, avg, stats->max, mdev);
    }
}
