#include "ft_ping.h"

const t_ping_opt g_options[] = {
    { "verbose",  'v', ARG_NONE, handle_verbose,  "verbose output", NULL },
    { "quiet",    'q', ARG_NONE, handle_quiet,    "quiet output",   NULL },
    { "help",     '?', ARG_NONE, handle_help,     "print help and exit", NULL },
    { "ttl",       0,  ARG_REQ,  handle_ttl,      "define time to live", "N" },
    { "count",    'c', ARG_REQ,  handle_count,    "stop after <N> replies", "N" },
    { "interval", 'i', ARG_REQ,  handle_interval, "wait <SEC> seconds", "SEC" },
    { "size",     's', ARG_REQ,  handle_size,     "data size", "N" },
    { "timeout",  'w', ARG_REQ,  handle_timeout,  "timeout", "N" },
    { NULL, 0, ARG_NONE, NULL, NULL, NULL }
};

