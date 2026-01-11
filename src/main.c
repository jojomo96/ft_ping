#include "ft_ping.h"
#include "ft_messages.h"
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

void handle_interrupt(int sig) {
    (void)sig;
    should_stop = 1;
}

/* Helper to handle error packets (Type 3 & 11) */
static int handle_error_packet(const struct ip *ip, const struct my_icmp_header *icmp, int pid) {
    /* Skip ICMP Error Header (8 bytes) to get Original IP */
    const struct ip *orig_ip = (const struct ip *)((char *)icmp + 8);
    int orig_ip_len = orig_ip->ip_hl * 4;

    /* Skip Original IP to get Original ICMP (contains the ID we sent) */
    const struct my_icmp_header *orig_icmp = (const struct my_icmp_header *)((char *)orig_ip + orig_ip_len);

    /* Does the ID match our process? */
    if (ntohs(orig_icmp->id) != (pid & 0xFFFF))
        return 0;

    if (flags.verbose) {
        char src_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip->ip_src, src_str, sizeof(src_str));
        printf("From %s: icmp_seq=%d ", src_str, ntohs(orig_icmp->sequence));

        if (icmp->type == ICMP_TIME_EXCEEDED)
            printf("Time to live exceeded\n");
        else if (icmp->type == ICMP_DEST_UNREACH)
            printf("Destination Host Unreachable\n");
        else
            printf("ICMP Error type=%d code=%d\n", icmp->type, icmp->code);
    }
    return 1;
}

int send_packet(int sock, int seq, int pid, char *packet) {
    size_t pack_size = sizeof(struct my_icmp_header) + flags.payload_size;
    ft_memset(packet, 0, pack_size);

    struct my_icmp_header *icmp = (struct my_icmp_header *)packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->id = htons(pid & 0xFFFF);
    icmp->sequence = htons(seq);
    icmp->checksum = 0;

    /* Fill Payload: Use ft_memcpy for alignment safety */
    size_t offset = sizeof(struct my_icmp_header);
    if ((size_t)flags.payload_size >= sizeof(struct timeval)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ft_memcpy(packet + offset, &tv, sizeof(tv));
    }

    /* Fill remainder with pattern */
    size_t i = offset + sizeof(struct timeval);
    while (i < pack_size) {
        packet[i] = (char)('!' + (i % 56));
        i++;
    }

    icmp->checksum = checksum(packet, pack_size);

    if (sendto(sock, packet, pack_size, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        if (!flags.quiet)
            printf("ft_ping: sendto: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* ** recv_packet now relies on the socket timeout (SO_RCVTIMEO) set in main.
** It will block for at most 1 second (or interval) then return.
*/
void recv_packet(int sock, int pid, char *buf, size_t buf_len) {
    struct msghdr msg = {0};
    struct iovec iov = {.iov_base = buf, .iov_len = buf_len};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    /* Loop until a valid packet is found or timeout/interrupt occurs */
    while (!should_stop) {
        ssize_t bytes = recvmsg(sock, &msg, 0);

        if (bytes < 0) {
            /* Timeout (EAGAIN/EWOULDBLOCK) or Interrupted (EINTR) */
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            perror("recvmsg");
            return;
        }

        struct ip *ip = (struct ip *)buf;
        int hlen = ip->ip_hl * 4;

        if (bytes < hlen + (int)sizeof(struct my_icmp_header))
            continue;

        struct my_icmp_header *icmp = (struct my_icmp_header *)(buf + hlen);

        /* Ignore our own ECHO REQUEST (Loopback) */
        if (icmp->type == ICMP_ECHO)
            continue;

        /* Case 1: Echo Reply */
        if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->id) == (pid & 0xFFFF)) {
            g_stats.rx++;
            double rtt = 0.0;

            /* Calculate RTT safely */
            size_t min_len = sizeof(struct my_icmp_header) + sizeof(struct timeval);
            if ((size_t)(bytes - hlen) >= min_len && (size_t)flags.payload_size >= sizeof(struct timeval)) {
                struct timeval sent_tv, curr_tv;
                ft_memcpy(&sent_tv, buf + hlen + sizeof(struct my_icmp_header), sizeof(struct timeval));
                gettimeofday(&curr_tv, NULL);
                rtt = (double)(curr_tv.tv_sec - sent_tv.tv_sec) * 1000.0 +
                      (double)(curr_tv.tv_usec - sent_tv.tv_usec) / 1000.0;
            }
            if (rtt < 0) rtt = 0;
            update_stats(rtt);

            if (!flags.quiet) {
                char from[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &dest_addr.sin_addr, from, sizeof(from));
                printf("%zd bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                       bytes - hlen, from, ntohs(icmp->sequence), ip->ip_ttl, rtt);
            }
            return; /* Got the packet, return to main loop */
        }
        /* Case 2: ICMP Error (TTL Exceeded / Unreachable) */
        else if (icmp->type == ICMP_TIME_EXCEEDED || icmp->type == ICMP_DEST_UNREACH) {
            if (handle_error_packet(ip, icmp, pid))
                return; /* Error handled, return to main loop */
        }
    }
}

void ping_loop(int sock) {
    int pid = getpid();
    int seq = 0;
    /* Stack allocation (removed malloc) */
    char packet[65535] __attribute__((aligned(8)));
    char recv_buf[4096] __attribute__((aligned(8)));

    gettimeofday(&g_stats.start_tv, NULL);

    while (!should_stop) {
        if (flags.count > 0 && seq >= flags.count) break;

        double t_start = get_time_ms();

        if (send_packet(sock, seq, pid, packet) == 0)
            g_stats.tx++;

        /* ** Try to receive packets.
        ** This will block for at most 'interval' seconds due to SO_RCVTIMEO.
        */
        recv_packet(sock, pid, recv_buf, sizeof(recv_buf));

        seq++;

        /* Sleep only if we processed the packet faster than the interval */
        double t_elapsed = get_time_ms() - t_start;
        if (t_elapsed < flags.interval_ms && !should_stop)
            usleep((useconds_t)((flags.interval_ms - t_elapsed) * 1000));
    }
}

int main(int argc, char **argv) {
    /* Use signal (allowed function) */
    signal(SIGINT, handle_interrupt);

    flags.ttl = 64;
    flags.interval_ms = 1000;
    flags.payload_size = 56;

    parse_args(argc, argv);
    resolve_destination(target);

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        perror("ft_ping: socket");
        fprintf(stderr, "Hint: Run with sudo\n");
        exit(1);
    }

    /* Configure Timeout using SO_RCVTIMEO (Allowed) */
    struct timeval tv_out;
    tv_out.tv_sec = flags.interval_ms / 1000;
    tv_out.tv_usec = (flags.interval_ms % 1000) * 1000;
    /* Safety: Ensure at least a small timeout so recvmsg doesn't block forever */
    if (tv_out.tv_sec == 0 && tv_out.tv_usec == 0) tv_out.tv_usec = 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        perror("setsockopt timeout");
        exit(1);
    }

    if (flags.ttl > 0 && setsockopt(sock, IPPROTO_IP, IP_TTL, &flags.ttl, sizeof(flags.ttl)) < 0)
        perror("setsockopt ttl");

    char ip_s[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest_addr.sin_addr, ip_s, sizeof(ip_s));

    printf("PING %s (%s): %d data bytes\n", target, ip_s, flags.payload_size);

    ping_loop(sock);
    print_stats(&g_stats);

    close(sock);
    return 0;
}