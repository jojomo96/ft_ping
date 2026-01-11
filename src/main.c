#include "ft_ping.h"
#include "ft_messages.h"
#include <signal.h>
#include <sys/time.h>
#include <math.h> // For sqrt in stddev
#include <string.h>

/* --- Helpers --- */

void handle_interrupt(const int sig) {
    (void) sig;
    should_stop = 1;
}

int send_packet(const int sock, int seq, const int pid, char *packet) {
    const size_t size = sizeof(struct icmp_header) + flags.payload_size;
    ft_memset(packet, 0, size);

    struct icmp_header *icmp = (struct icmp_header *) packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    /* FIX: Mask PID to 16 bits to match header field size */
    icmp->id = htons(pid & 0xFFFF);
    icmp->sequence = htons(seq);

    /* Fill Payload: Timestamp first */
    if ((size_t) flags.payload_size >= sizeof(struct timeval)) {
        struct timeval *tv = (struct timeval *) (packet + sizeof(struct icmp_header));
        gettimeofday(tv, NULL);
    }

    /* Fill Payload: Pattern */
    size_t i = sizeof(struct icmp_header) + sizeof(struct timeval);
    while (i < size) {
        packet[i] = (char) ('!' + (i % 56));
        i++;
    }

    icmp->checksum = checksum(packet, size);

    if (sendto(sock, packet, size, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        if (!flags.quiet) printf("ft_ping: sendto: %s\n", strerror(errno));
        return (-1);
    }
    return (0);
}

void recv_packet(const int sock, const int pid) {
    char buf[4096] __attribute__((aligned(8)));
    struct msghdr msg = {0};
    struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    /* Loop until we get OUR packet, or timeout/error */
    while (1) {
        const ssize_t ret = recvmsg(sock, &msg, 0);
        if (ret < 0) return; /* Timeout (EAGAIN) or Signal */

        const struct ip *ip = (struct ip *) buf;
        const int hlen = ip->ip_hl * 4;

        /* Check if packet is large enough */
        if (ret < hlen + (int) sizeof(struct icmp_header)) continue;

        struct icmp_header *icmp = (struct icmp_header *) (buf + hlen);

        /* FIX: Compare ID masked to 16 bits */
        if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->id) == (pid & 0xFFFF)) {
            g_stats.rx++;

            double rtt = 0.0;
            if ((size_t) flags.payload_size >= sizeof(struct timeval) && (ret - hlen) >= (int) sizeof(struct
                    icmp_header) + (int) sizeof(struct timeval)) {
                const struct timeval *sent = (struct timeval *) (buf + hlen + sizeof(struct icmp_header));
                struct timeval now;
                gettimeofday(&now, NULL);
                rtt = ((now.tv_sec - sent->tv_sec) * 1000.0) +
                      ((now.tv_usec - sent->tv_usec) / 1000.0);
            }
            if (rtt < 0) rtt = 0; /* Safety against clock skew */
            update_stats(rtt);

            if (!flags.quiet) {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &dest_addr.sin_addr, ip_str, sizeof(ip_str));
                printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                       ret - hlen, ip_str, ntohs(icmp->sequence), ip->ip_ttl, rtt
                );
            }
            return; /* Success */
        }
    }
}

void ping_loop(const int sock) {
    const int pid = getpid();
    int seq = 0;

    /* Allocate packet buffer once to prevent memory leaks */
    char *packet = malloc(sizeof(struct icmp_header) + flags.payload_size);
    if (!packet) {
        perror("malloc");
        return;
    }

    gettimeofday(&g_stats.start_tv, NULL);

    while (!should_stop) {
        if (flags.count > 0 && seq >= flags.count) break;

        const double start_time = get_time_ms();

        if (send_packet(sock, seq, pid, packet) == 0)
            g_stats.tx++;

        recv_packet(sock, pid);
        seq++;

        /* Smart Interval Sleep */
        if (!should_stop && (flags.count <= 0 || seq < flags.count)) {
            double elapsed = get_time_ms() - start_time;
            double sleep_time = (double) flags.interval_ms - elapsed;

            if (sleep_time > 0)
                usleep((useconds_t) (sleep_time * 1000.0));
        }
    }
    free(packet);
}

int main(const int argc, char **argv) {
    struct timeval tv_out;

    parse_args(argc, argv);
    resolve_destination(target);

    const int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("ft_ping: socket");
        /* Hint for user if permission denied */
        if (errno == EPERM || errno == EACCES)
            fprintf(stderr, "Hint: Run with sudo or setuid\n");
        exit(1);
    }

    /* Configure Socket Timeout (Match interval, min 1s) */
    tv_out.tv_sec = (flags.interval_ms < 1000) ? 1 : flags.interval_ms / 1000;
    tv_out.tv_usec = (flags.interval_ms % 1000) * 1000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        perror("ft_ping: setsockopt timeout");
        close(sockfd);
        exit(1);
    }

    /* Configure TTL */
    if (flags.ttl > 0) {
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &flags.ttl, sizeof(flags.ttl)) < 0) {
            perror("ft_ping: setsockopt ttl");
        }
    }

    signal(SIGINT, handle_interrupt);

    /* Header Output */
    char ip_s[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest_addr.sin_addr, ip_s, sizeof(ip_s));
    printf("PING %s (%s): %d data bytes\n", target, ip_s, flags.payload_size);

    ping_loop(sockfd);
    print_stats(&g_stats);

    close(sockfd);
    return (0);
}
