#include "ft_ping.h"

uint16_t checksum(void *data, int len) {
    uint32_t sum = 0;
    uint16_t *ptr = data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1)
        sum += *(uint8_t *)ptr;

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (uint16_t)(~sum);
}

void resolve_destination(const char *hostname) {
    struct addrinfo hints, *res;
    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Error resolving hostname: %s\n", hostname);
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
    if (rtt < 0)
        return;
    /* update_stats expects g_stats.rx to have been incremented by the caller */
    if (g_stats.rx == 1 || rtt < g_stats.min) g_stats.min = rtt;
    if (g_stats.rx == 1 || rtt > g_stats.max) g_stats.max = rtt;
    g_stats.sum += rtt;
    g_stats.sq_sum += rtt * rtt;
}
