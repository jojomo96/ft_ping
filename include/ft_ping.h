#ifndef HEADER_H
#define HEADER_H

#include "ft_ping_error.h"

#include "libft/ft_getopt.h"
#include "libft/libft.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <limits.h>
#include <sys/errno.h>


#define ICMP_ECHO       8
#define ICMP_ECHOREPLY  0
#define PAYLOAD_SIZE 56

struct icmp_header {
    uint8_t type; // ICMP message type
    uint8_t code; // Error code
    uint16_t checksum; // Checksum for ICMP header + data
    uint16_t id; // Identifier (usually PID)
    uint16_t sequence; // Sequence number
} __attribute__((packed));

typedef struct s_flags {
    int count; // from -c
    int interval_ms; // from -i
    int timeout; // from -W
    int verbose; // from -v
    int ttl; // from -t
    int payload_size; // from -s
    int quiet; // from -q
} t_flags;

extern char *target;
extern t_flags flags;
extern struct sockaddr_in dest_addr;
extern int should_stop;

uint16_t checksum(void *data, int len);

void parse_flags_and_target(int argc, char **argv);

void resolve_destination(const char *hostname);

void set_socket_timeout(int sockfd);

int create_raw_socket_with_timeout();

void handle_interrupt(int sig);

void parse_args(int argc, char **argv);

#endif //HEADER_H
