#include "ft_ping.h"
#include "ft_messages.h"


static long long parse_ll_or_fatal(const char *val, t_msg_id invalid_id) {
    long long out;

    if (!ft_atoll_checked(val, &out))
        ping_fatal(invalid_id, val);
    return out;
}

void handle_verbose(const char *val) {
    (void) val;
    flags.verbose = 1;
}

void handle_quiet(const char *val) {
    (void) val;
    flags.quiet = 1;
}

void handle_help(const char *val) {
    (void) val;
    /* Usually usage() prints the help text, exit is handled there or here */
    ft_usage(0);
}

void handle_count(const char *val) {
    const long long count = parse_ll_or_fatal(val, MSG_ERR_INVALID_COUNT);

    if (count < 1 || count > INT_MAX)
        ping_fatal(MSG_ERR_COUNT_RANGE, val);

    flags.count = (int)count;
}

void handle_ttl(const char *val) {
    const long long ttl = parse_ll_or_fatal(val, MSG_ERR_INVALID_TTL);

    if (ttl < 0 || ttl > 255)
        ping_fatal(MSG_ERR_TTL_RANGE, val);

    flags.ttl = (int)ttl;
}

void handle_size(const char *val) {
    const long long size = parse_ll_or_fatal(val, MSG_ERR_INVALID_SIZE);

    /* 56 data bytes is default; payload must be within IPv4 limits.
     * 65507 is the largest UDP payload, good practical upper bound here too. */
    if (size < 0)
        ping_fatal(MSG_ERR_SIZE_RANGE, val);
    if (size > 65507)
        ping_fatal(MSG_ERR_INVALID_SIZE, val);

    flags.payload_size = (int)size;
}

void handle_timeout(const char *val) {
    const long long timeout = parse_ll_or_fatal(val, MSG_ERR_INVALID_TIMEOUT);

    if (timeout < 0 || timeout > INT_MAX)
        ping_fatal(MSG_ERR_INVALID_TIMEOUT, val);

    flags.timeout = (int)timeout;
}

void handle_interval(const char *val) {
    if (!ft_str_is_double(val))
        ping_fatal(MSG_ERR_INVALID_INTERVAL, val);

    double d = ft_atof(val);

    /* reject NaN/inf explicitly */
    if (isnan(d) || isinf(d))
        ping_fatal(MSG_ERR_INVALID_INTERVAL, val);

    if (d < 0.002 && d != 0.0) /* Safety limit for non-flood */
        ping_fatal(MSG_ERR_INTERVAL_SHORT, val);

    if (d > INT_MAX / 1000.0)
        ping_fatal(MSG_ERR_INVALID_INTERVAL, val);

    flags.interval_ms = (int) (d * 1000.0);
}
