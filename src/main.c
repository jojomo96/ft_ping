#include "ft_ping.h"
#include <signal.h>
#include <sys/time.h>
#include <math.h> // For sqrt in stddev
#include <string.h>

static t_stats g_stats = {0, 0, 0.0, 0.0, 0.0, 0.0, {0, 0}};

/* --- Helpers --- */

void handle_interrupt(int sig) {
    (void) sig;
    should_stop = 1;
}

double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

void update_stats(double rtt) {
    if (g_stats.rx == 1 || rtt < g_stats.min) g_stats.min = rtt;
    if (g_stats.rx == 1 || rtt > g_stats.max) g_stats.max = rtt;
    g_stats.sum += rtt;
    g_stats.sq_sum += rtt * rtt;
}

void print_stats(void) {
    double total = get_time_ms() -
                   ((g_stats.start_tv.tv_sec * 1000.0) + (g_stats.start_tv.tv_usec / 1000.0));
    double loss = 0;

    if (g_stats.tx > 0)
        loss = ((g_stats.tx - g_stats.rx) * 100.0) / g_stats.tx;

    printf("\n--- %s ping statistics ---\n", target);
    printf("%ld packets transmitted, %ld received, %.0f%% packet loss, time %.0fms\n",
           g_stats.tx, g_stats.rx, loss, total
    );

    if (g_stats.rx > 0) {
        double avg = g_stats.sum / g_stats.rx;
        double mdev = sqrt((g_stats.sq_sum / g_stats.rx) - (avg * avg));
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
               g_stats.min, avg, g_stats.max, mdev
        );
    }
}

/* --- Core Logic --- */

int send_packet(int sock, int seq, int pid, char *packet) {
    struct icmp_header *icmp;
    size_t size;
    size_t i;

    size = sizeof(struct icmp_header) + flags.payload_size;
    ft_memset(packet, 0, size);

    icmp = (struct icmp_header *) packet;
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
    i = sizeof(struct icmp_header) + sizeof(struct timeval);
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

void recv_packet(int sock, int pid) {
    char buf[4096] __attribute__((aligned(8)));
    struct msghdr msg = {0};
    struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};
    ssize_t ret;
    struct ip *ip;
    struct icmp_header *icmp;
    int hlen;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    /* Loop until we get OUR packet, or timeout/error */
    while (1) {
        ret = recvmsg(sock, &msg, 0);
        if (ret < 0) return; /* Timeout (EAGAIN) or Signal */

        ip = (struct ip *) buf;
        hlen = ip->ip_hl * 4;

        /* Check if packet is large enough */
        if (ret < hlen + (int) sizeof(struct icmp_header)) continue;

        icmp = (struct icmp_header *) (buf + hlen);

        /* FIX: Compare ID masked to 16 bits */
        if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->id) == (pid & 0xFFFF)) {
            g_stats.rx++;

            double rtt = 0.0;
            if ((size_t) flags.payload_size >= sizeof(struct timeval) && (ret - hlen) >= (int) sizeof(struct
                    icmp_header) + (int) sizeof(struct timeval)) {
                struct timeval *sent = (struct timeval *) (buf + hlen + sizeof(struct icmp_header));
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

void ping_loop(int sock) {
    int pid = getpid();
    int seq = 0;
    char *packet;
    double start_time;

    /* Allocate packet buffer once to prevent memory leaks */
    packet = malloc(sizeof(struct icmp_header) + flags.payload_size);
    if (!packet) {
        perror("malloc");
        return;
    }

    gettimeofday(&g_stats.start_tv, NULL);

    while (!should_stop) {
        if (flags.count > 0 && seq >= flags.count) break;

        start_time = get_time_ms();

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

int main(int argc, char **argv) {
    int sockfd;
    struct timeval tv_out;

    parse_args(argc, argv);
    resolve_destination(target);

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
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
    print_stats();

    close(sockfd);
    return (0);
}
