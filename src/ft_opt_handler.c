#include "ft_ping.h"

void handle_verbose(const char *val) { (void)val; flags.verbose = 1; }
void handle_quiet(const char *val)   { (void)val; flags.quiet = 1; }
void handle_help(const char *val)    { (void)val; exit(0); }

void handle_count(const char *val) {
    if (!ft_str_is_number(val))
        ping_error_exit("invalid count", val);
    flags.count = ft_atoi(val);
    if (flags.count < 1)
        ping_error_exit("count must be >= 1", val);
}

void handle_ttl(const char *val) {
    if (!ft_str_is_number(val))
        ping_error_exit("invalid TTL", val);
    flags.ttl = ft_atoi(val);
    if (flags.ttl < 0 || flags.ttl > 255)
        ping_error_exit("ttl out of range", val);
}

void handle_size(const char *val) {
    if (!ft_str_is_number(val))
        ping_error_exit("invalid packet size", val);
    flags.payload_size = ft_atoi(val);
    if (flags.payload_size < 0)
        ping_error_exit("size cannot be negative", val);
}

void handle_timeout(const char *val) {
    if (!ft_str_is_number(val))
        ping_error_exit("invalid timeout", val);
    flags.timeout = ft_atoi(val);
}

void handle_interval(const char *val) {
    if (!ft_str_is_double(val))
        ping_error_exit("invalid interval", val);
    double d = ft_atof(val);
    if (d < 0.002 && d != 0.0) /* Root can go faster, but let's stick to safe limits */
        ping_error_exit("interval too short", val);
    flags.interval_ms = (int)(d * 1000.0);
}