/* include/ft_ping.h */
#ifndef HEADER_H
#define HEADER_H

#include <stdint.h>
#include <signal.h>
#include <sys/time.h>
#include <netinet/in.h>

/* Custom ICMP defines if not available */
#ifndef ICMP_ECHO
# define ICMP_ECHO 8
#endif
#ifndef ICMP_ECHOREPLY
# define ICMP_ECHOREPLY 0
#endif
#ifndef ICMP_DEST_UNREACH
# define ICMP_DEST_UNREACH 3
#endif
#ifndef ICMP_TIME_EXCEEDED
# define ICMP_TIME_EXCEEDED 11
#endif

/* Re-definition of ICMP header to avoid dependency issues */
struct my_icmp_header {
    uint8_t type;      // 8 for Request, 0 for Reply, 11 for Time Exceeded, etc.
    uint8_t code;      // Usually 0 for Echo
    uint16_t checksum; // Critical for error checking
    uint16_t id;       // Unique ID to identify THIS ping process
    uint16_t sequence; // 1, 2, 3... to track packet loss/ordering
} __attribute__((packed));

/* Globals & Structs */
typedef struct s_flags {
    int count;
    int interval_ms;
    int timeout;
    int verbose;
    int ttl;
    int payload_size;
    int quiet;
} t_flags;

/* Global variables */
extern char *target;
extern t_flags flags;
extern struct sockaddr_in dest_addr;
extern volatile sig_atomic_t should_stop;

/* Stats structure */
typedef struct s_stats {
    long    tx;
    long    rx;
    double  min;
    double  max;
    double  sum;
    double  sq_sum;
    struct  timeval start_tv;
} t_stats;

extern t_stats g_stats;

/* Functions */
double   get_time_ms(void);
void     update_stats(double rtt);
uint16_t checksum(void *data, int len);
void     resolve_destination(const char *hostname);
void     ft_usage(int exit_code);
void     parse_args(int argc, char **argv);

/* Option Handlers */
typedef void (*t_opt_handler)(const char *val);
typedef enum { ARG_NONE, ARG_REQ } t_arg_type;
typedef struct s_ping_opt {
    const char      *long_name;
    char             short_name;
    t_arg_type       type;
    t_opt_handler    handler;
    const char      *desc;
    const char      *arg_label;
} t_ping_opt;

extern const t_ping_opt g_options[];

/* Handlers */
void handle_verbose(const char *val);
void handle_quiet(const char *val);
void handle_help(const char *val);
void handle_count(const char *val);
void handle_ttl(const char *val);
void handle_size(const char *val);
void handle_timeout(const char *val);
void handle_interval(const char *val);

#endif