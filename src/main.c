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
    const struct ip *orig_ip = (const struct ip *)((char *)icmp + 8);
    int orig_ip_len = orig_ip->ip_hl * 4;

    const struct my_icmp_header *orig_icmp = (const struct my_icmp_header *)((char *)orig_ip + orig_ip_len);

    if (ntohs(orig_icmp->id) != (pid & 0xFFFF))
        return 0;

    if (flags.verbose) {
        char src_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip->ip_src, src_str, sizeof(src_str));

        if (icmp->type == ICMP_TIME_EXCEEDED)
            ping_msg(MSG_PING_FROM, src_str, ntohs(orig_icmp->sequence), "Time to live exceeded");
        else if (icmp->type == ICMP_DEST_UNREACH)
            ping_msg(MSG_PING_FROM, src_str, ntohs(orig_icmp->sequence), "Destination Host Unreachable");
        else {
            /* Minimal: keep message generic without extra formatting */
            ping_msg(MSG_PING_FROM, src_str, ntohs(orig_icmp->sequence), "ICMP Error");
        }
    }
    return 1;
}

int send_packet(int sock, int seq, int pid, char *packet) {
    size_t pack_size = sizeof(struct my_icmp_header) + (size_t)flags.payload_size;
    ft_memset(packet, 0, pack_size);

    struct my_icmp_header *icmp = (struct my_icmp_header *)packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->id = htons(pid & 0xFFFF);
    icmp->sequence = htons(seq);
    icmp->checksum = 0;

    size_t offset = sizeof(struct my_icmp_header);
    if ((size_t)flags.payload_size >= sizeof(struct timeval)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ft_memcpy(packet + offset, &tv, sizeof(tv));
    }

    size_t i = offset + sizeof(struct timeval);
    while (i < pack_size) {
        packet[i] = (char)('!' + (i % 56));
        i++;
    }

    icmp->checksum = checksum(packet, (int)pack_size);

    if (sendto(sock, packet, pack_size, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        if (!flags.quiet)
            ping_msg(MSG_ERR_SENDTO, strerror(errno));
        return -1;
    }
    return 0;
}

void recv_packet(int sock, int pid, char *buf, size_t buf_len) {
    struct msghdr msg = (struct msghdr){0};
    struct iovec iov = (struct iovec){.iov_base = buf, .iov_len = buf_len};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while (!should_stop) {
        ssize_t bytes = recvmsg(sock, &msg, 0);

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            ping_msg(MSG_ERR_RECVMSG, strerror(errno));
            return;
        }

        struct ip *ip = (struct ip *)buf;
        int hlen = ip->ip_hl * 4;

        if (bytes < hlen + (int)sizeof(struct my_icmp_header))
            continue;

        struct my_icmp_header *icmp = (struct my_icmp_header *)(buf + hlen);

        if (icmp->type == ICMP_ECHO)
            continue;

        if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->id) == (pid & 0xFFFF)) {
            g_stats.rx++;
            double rtt = 0.0;

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
                ping_msg(MSG_PING_REPLY, (long)(bytes - hlen), from, ntohs(icmp->sequence), ip->ip_ttl, rtt);
            }
            return;
        } else if (icmp->type == ICMP_TIME_EXCEEDED || icmp->type == ICMP_DEST_UNREACH) {
            if (handle_error_packet(ip, icmp, pid))
                return;
        }
    }
}

void ping_loop(int sock) {
    int pid = getpid();
    int seq = 0;
    char packet[65535] __attribute__((aligned(8)));
    char recv_buf[4096] __attribute__((aligned(8)));

    gettimeofday(&g_stats.start_tv, NULL);

    while (!should_stop) {
        if (flags.count > 0 && seq >= flags.count) break;

        double t_start = get_time_ms();

        if (send_packet(sock, seq, pid, packet) == 0)
            g_stats.tx++;

        recv_packet(sock, pid, recv_buf, sizeof(recv_buf));

        seq++;

        double t_elapsed = get_time_ms() - t_start;
        if (t_elapsed < flags.interval_ms && !should_stop)
            usleep((useconds_t)((flags.interval_ms - t_elapsed) * 1000.0));
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_interrupt);

    flags.ttl = 64;
    flags.interval_ms = 1000;
    flags.payload_size = 56;

    parse_args(argc, argv);
    resolve_destination(target);

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        ping_fatal(MSG_ERR_SOCKET, strerror(errno));
    }

    struct timeval tv_out;
    tv_out.tv_sec = flags.interval_ms / 1000;
    tv_out.tv_usec = (flags.interval_ms % 1000) * 1000;
    if (tv_out.tv_sec == 0 && tv_out.tv_usec == 0) tv_out.tv_usec = 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        ping_fatal(MSG_ERR_SETSOCKOPT_TIMEOUT, strerror(errno));
    }

    if (flags.ttl > 0 && setsockopt(sock, IPPROTO_IP, IP_TTL, &flags.ttl, sizeof(flags.ttl)) < 0) {
        ping_msg(MSG_ERR_SETSOCKOPT_TTL, strerror(errno));
    }

    char ip_s[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest_addr.sin_addr, ip_s, sizeof(ip_s));

    ping_msg(MSG_PING_HEADER, target, ip_s, flags.payload_size);

    ping_loop(sock);
    print_stats(&g_stats);

    close(sock);
    return 0;
}
