#include "ft_ping.h"
#include "ft_messages.h"
#include <string.h>


/*
** Function: checksum
** ------------------
** Calculates the 16-bit One's Complement checksum for ICMP/IP headers.
**
** This implementation is designed for safety and portability:
** 1. Alignment Safety: Instead of casting `void *data` directly to `uint16_t*`
** (which causes undefined behavior or SIGBUS on non-aligned memory on
** strict architectures like ARM/SPARC), we use `ft_memcpy` to copy
** bytes into a local `uint16_t` variable.
** 2. Endianness: The logic handles both Big and Little Endian architectures
** correctly by processing the buffer as a stream of bytes.
**
** @param data  Pointer to the buffer to checksum.
** @param len   Length of the buffer in bytes.
**
** @return      The 1's complement of the 1's complement sum (network byte order).
*/
uint16_t checksum(void *b, int len) {
    unsigned char *buf = b;
    unsigned int sum = 0;
    uint16_t word;

    while (len > 1) {
        ft_memcpy(&word, buf, 2);
        sum += word;
        buf += 2;
        len -= 2;
    }

    if (len == 1) {
        word = 0;
        ft_memcpy(&word, buf, 1);
        sum += word;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t) (~sum);
}

void resolve_destination(const char *hostname) {
    struct addrinfo hints, *res;
    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        ping_fatal(MSG_ERR_UNKNOWN_HOST, hostname);
    }
    ft_memcpy(&dest_addr, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);
}

double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

void update_stats(const double rtt) {
    if (rtt < 0) return;
    if (g_stats.rx == 1 || rtt < g_stats.min) g_stats.min = rtt;
    if (g_stats.rx == 1 || rtt > g_stats.max) g_stats.max = rtt;
    g_stats.sum += rtt;
    g_stats.sq_sum += rtt * rtt;
}
