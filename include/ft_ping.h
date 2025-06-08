#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ICMP_ECHO       8
#define ICMP_ECHOREPLY  0
#define PAYLOAD_SIZE 56

struct icmp_header {
    uint8_t  type;       // ICMP message type
    uint8_t  code;       // Error code
    uint16_t checksum;   // Checksum for ICMP header + data
    uint16_t id;         // Identifier (usually PID)
    uint16_t sequence;   // Sequence number
} __attribute__((packed));

typedef struct s_flags
{
    int verbose;
} t_flags;

#endif //HEADER_H
