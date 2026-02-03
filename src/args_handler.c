#include "ft_ping.h"
#include "ft_messages.h"


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
    if (!ft_str_is_number(val))
        ping_fatal(MSG_ERR_INVALID_COUNT, val);

    flags.count = ft_atoi(val);

    if (flags.count < 1)
        ping_fatal(MSG_ERR_COUNT_RANGE, val);
}

void handle_ttl(const char *val) {
    if (!ft_str_is_number(val))
        ping_fatal(MSG_ERR_INVALID_TTL, val);

    flags.ttl = ft_atoi(val);

    if (flags.ttl < 0 || flags.ttl > 255)
        ping_fatal(MSG_ERR_TTL_RANGE, val);
}

void handle_size(const char *val) {
    if (!ft_str_is_number(val))
        ping_fatal(MSG_ERR_INVALID_SIZE, val);

    const int size = ft_atoi(val);
    if (size < 0 || size > 65507) // Max TCP/IP payload
        ping_fatal(MSG_ERR_SIZE_RANGE, val);

    flags.payload_size = size;
}

void handle_timeout(const char *val) {
    if (!ft_str_is_number(val))
        ping_fatal(MSG_ERR_INVALID_TIMEOUT, val);

    flags.timeout = ft_atoi(val);

    if (flags.timeout < 0 || flags.timeout > INT_MAX)
        ping_fatal(MSG_ERR_INVALID_TIMEOUT, val);
}

void handle_interval(const char *val) {
    if (!ft_str_is_double(val))
        ping_fatal(MSG_ERR_INVALID_INTERVAL, val);

    double d = ft_atof(val);

    if (d < 0.002 && d != 0.0) /* Safety limit for non-flood */
        ping_fatal(MSG_ERR_INTERVAL_SHORT, val);

    if (d > INT_MAX / 1000.0)
        ping_fatal(MSG_ERR_INVALID_INTERVAL, val);

    flags.interval_ms = (int) (d * 1000.0);
}
