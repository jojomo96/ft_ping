/* src/main.c */
#include "ft_ping.h"
#include "ft_messages.h"
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>

/* --- Helpers --- */

void handle_interrupt(const int sig) {
    (void) sig;
    should_stop = 1;
}

/* ** Helper to analyze and print ICMP error packets (Type 3 and 11).
** Returns 1 if the error packet belongs to our process, 0 otherwise.
*/
static int handle_error_packet(const struct ip *ip, const struct icmp_header *icmp, int pid) {
    /* * Structure of an Error Packet:
     * [ IP Header ] [ ICMP Error Header (8 bytes) ] [ Original IP Header ] [ Original ICMP Header (8 bytes) ]
     */
    
    /* 1. Skip the ICMP Error Header to get to the "Original IP Header" */
    const struct ip *inner_ip = (const struct ip *)((char *)icmp + 8);
    
    /* 2. Calculate length of Inner IP Header to find "Original ICMP Header" */
    int inner_ip_hl = inner_ip->ip_hl * 4;
    const struct icmp_header *inner_icmp = (const struct icmp_header *)((char *)inner_ip + inner_ip_hl);

    /* 3. Check if the nested packet ID matches our process */
    if (ntohs(inner_icmp->id) != (pid & 0xFFFF))
        return (0);

    /* 4. Subject Requirement: Only show results in case of error if -v is set */
    if (flags.verbose) {
        char src_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip->ip_src, src_str, sizeof(src_str));

        printf("From %s: icmp_seq=%d ", src_str, ntohs(inner_icmp->sequence));

        if (icmp->type == ICMP_TIME_EXCEEDED)
            printf("Time to live exceeded\n");
        else if (icmp->type == ICMP_DEST_UNREACH)
            printf("Destination Host Unreachable\n");
        else
            printf("ICMP Error type %d code %d\n", icmp->type, icmp->code);
    }

    return (1);
}

int send_packet(const int sock, int seq, const int pid, char *packet) {
    const size_t size = sizeof(struct icmp_header) + flags.payload_size;
    ft_memset(packet, 0, size);

    struct icmp_header *icmp = (struct icmp_header *) packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    /* Mask PID to 16 bits to match header field size */
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
    /* Stack buffer for receiving packets */
    char buf[4096] __attribute__((aligned(8)));
    struct msghdr msg = {0};
    struct iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while (1) {
        const ssize_t ret = recvmsg(sock, &msg, 0);
        if (ret < 0) return; /* Timeout or Interrupted */

        const struct ip *ip = (struct ip *) buf;
        const int hlen = ip->ip_hl * 4;

        /* Basic validation */
        if (ret < hlen + (int) sizeof(struct icmp_header)) continue;

        struct icmp_header *icmp = (struct icmp_header *) (buf + hlen);

        /* DEBUG PRINT */
        if (flags.verbose) {
            printf("DEBUG: Recv Type=%d, ID=%d, Expected PID=%d\n",
                   icmp->type, ntohs(icmp->id), pid & 0xFFFF);
        }

        /* Case 1: Standard Echo Reply */
        if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->id) == (pid & 0xFFFF)) {
            g_stats.rx++;

            double rtt = 0.0;
            size_t min_size = sizeof(struct icmp_header) + sizeof(struct timeval);
            
            /* Calculate RTT if payload is sufficient */
            if ((size_t)flags.payload_size >= sizeof(struct timeval) && (size_t)(ret - hlen) >= min_size) {
                const struct timeval *sent = (struct timeval *) (buf + hlen + sizeof(struct icmp_header));
                struct timeval now;
                gettimeofday(&now, NULL);
                rtt = ((now.tv_sec - sent->tv_sec) * 1000.0) +
                      ((now.tv_usec - sent->tv_usec) / 1000.0);
            }
            if (rtt < 0) rtt = 0;
            update_stats(rtt);

            if (!flags.quiet) {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &dest_addr.sin_addr, ip_str, sizeof(ip_str));
                printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                       ret - hlen, ip_str, ntohs(icmp->sequence), ip->ip_ttl, rtt
                );
            }
            return;
        }
        
        /* Case 2: ICMP Errors (Time Exceeded / Dest Unreachable) */
        else if (icmp->type == ICMP_TIME_EXCEEDED || icmp->type == ICMP_DEST_UNREACH) {
            if (handle_error_packet(ip, icmp, pid))
                return; /* Packet processed, return to main loop */
        }
    }
}

void ping_loop(const int sock) {
    const int pid = getpid();
    int seq = 0;

    /* * REPLACEMENT: Use stack memory instead of malloc.
     * Max IPv4 packet size is 65535.
     */
    char packet[65535]; 

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
    /* No free(packet) needed */
}

int main(const int argc, char **argv) {
    struct timeval tv_out;

    parse_args(argc, argv);
    resolve_destination(target);

    const int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("ft_ping: socket");
        if (errno == EPERM || errno == EACCES)
            fprintf(stderr, "Hint: Run with sudo or setuid\n");
        exit(1);
    }

    tv_out.tv_sec = (flags.interval_ms < 1000) ? 1 : flags.interval_ms / 1000;
    tv_out.tv_usec = (flags.interval_ms % 1000) * 1000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        perror("ft_ping: setsockopt timeout");
        close(sockfd);
        exit(1);
    }

    if (flags.ttl > 0) {
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &flags.ttl, sizeof(flags.ttl)) < 0) {
            perror("ft_ping: setsockopt ttl");
        }
    }

    signal(SIGINT, handle_interrupt);

    char ip_s[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest_addr.sin_addr, ip_s, sizeof(ip_s));
    printf("PING %s (%s): %d data bytes\n", target, ip_s, flags.payload_size);

    ping_loop(sockfd);
    print_stats(&g_stats);

    close(sockfd);
    return (0);
}