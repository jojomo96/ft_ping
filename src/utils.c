#include "ft_ping.h"

/* ** Standard Checksum (RFC 1071) - Alignment Safe version
** Uses ft_memcpy to safely copy bytes into a 16-bit word before summing.
*/
uint16_t checksum(void *b, int len) {
    unsigned char *buf = b;
    unsigned int sum = 0;
    uint16_t word;

    while (len > 1) {
        ft_memcpy(&word, buf, 2); /* Safe copy 2 bytes */
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

    return (uint16_t)(~sum);
}

void resolve_destination(const char *hostname) {
    struct addrinfo hints, *res;
    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        fprintf(stderr, "ft_ping: unknown host %s\n", hostname);
        exit(1);
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