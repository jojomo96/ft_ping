#ifndef PING_MESSAGES_H
#define PING_MESSAGES_H

#include <stdarg.h>

/* ** Message IDs
** Add new messages here.
*/
typedef enum e_msg_id {
    MSG_USAGE_TITLE,        /* "Usage: ft_ping ..." */
    MSG_ERR_DEST_REQ,       /* "destination required" */
    MSG_ERR_MULTIPLE_DEST,  /* "multiple destinations provided" */
    MSG_ERR_UNEXPECTED_ARG, /* "unexpected argument" */
    MSG_ERR_UNKNOWN_OPT,    /* "unknown option -- '%c'" */
    MSG_ERR_UNRECOG_OPT,    /* "unrecognized option '%s'" */
    MSG_ERR_OPT_REQ_ARG,    /* "option requires an argument" */
    MSG_ERR_INVALID_NUM,    /* "invalid value '%s'..." */
    MSG_ERR_TTL_RANGE,      /* "ttl out of range" */

    MSG_ERR_INVALID_COUNT,    /* "invalid count: '%s'" */
    MSG_ERR_COUNT_RANGE,      /* "count must be >= 1: '%s'" */

    MSG_ERR_INVALID_TTL,      /* "invalid TTL: '%s'" */

    MSG_ERR_INVALID_SIZE,     /* "invalid packet size: '%s'" */
    MSG_ERR_SIZE_RANGE,       /* "size cannot be negative: '%s'" */

    MSG_ERR_INVALID_TIMEOUT,  /* "invalid timeout: '%s'" */

    MSG_ERR_INVALID_INTERVAL, /* "invalid interval: '%s'" */
    MSG_ERR_INTERVAL_SHORT,   /* "interval too short: '%s'" */

    MSG_ERR_INVALID_TYPE,     /* "invalid type: '%s'" */
} t_msg_id;

/*
** Output Functions
*/
void    ping_msg(t_msg_id id, ...);
void    ping_fatal(t_msg_id id, ...);

#endif