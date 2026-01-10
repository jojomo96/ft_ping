/* ************************************************************************** */
/* */
/* :::      ::::::::   */
/* main.c                                             :+:      :+:    :+:   */
/* +:+ +:+         +:+     */
/* By: ft_ping <ft@ping>                          +#+  +:+       +#+        */
/* +#+#+#+#+#+   +#+           */
/* Created: 2025/01/10 12:00:00 by ft_ping           #+#    #+#             */
/* Updated: 2025/01/10 12:00:00 by ft_ping          ###   ########.fr       */
/* */
/* ************************************************************************** */

#include "ft_ping.h"
#include <signal.h>
#include <sys/time.h>
#include <math.h> // For sqrt in stddev
#include <string.h>

// -- Globals (defined in header) --
char                *target = NULL;
t_flags             flags = {0};
struct sockaddr_in  dest_addr;
int                 should_stop = 0;

// -- Internal Stats Structure --
static struct {
    long    tx_count;
    long    rx_count;
    double  t_min;
    double  t_max;
    double  t_sum;
    double  t_sq_sum; // For standard deviation
    struct  timeval t_start;
} g_stats = {0, 0, 0.0, 0.0, 0.0, 0.0, {0, 0}};

/* --- Helpers --- */

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

static void update_stats(double rtt_ms) {
    if (g_stats.rx_count == 1) {
        g_stats.t_min = rtt_ms;
        g_stats.t_max = rtt_ms;
    } else {
        if (rtt_ms < g_stats.t_min) g_stats.t_min = rtt_ms;
        if (rtt_ms > g_stats.t_max) g_stats.t_max = rtt_ms;
    }
    g_stats.t_sum += rtt_ms;
    g_stats.t_sq_sum += rtt_ms * rtt_ms;
}

void print_final_stats(void) {
    double total_time = get_time_ms() - ((g_stats.t_start.tv_sec * 1000.0) + (g_stats.t_start.tv_usec / 1000.0));

    printf("\n--- %s ping statistics ---\n", target);
    printf("%ld packets transmitted, %ld received, %.0f%% packet loss, time %.0fms\n",
           g_stats.tx_count,
           g_stats.rx_count,
           ((g_stats.tx_count - g_stats.rx_count) * 100.0) / (g_stats.tx_count ? g_stats.tx_count : 1),
           total_time);

    if (g_stats.rx_count > 0) {
        double avg = g_stats.t_sum / g_stats.rx_count;
        double mdev = sqrt((g_stats.t_sq_sum / g_stats.rx_count) - (avg * avg));
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
               g_stats.t_min, avg, g_stats.t_max, mdev);
    }
}

void interrupt_handler(int sig) {
    (void)sig;
    should_stop = 1; // Breaks the main loop
}

/* --- Core Logic --- */

static void send_ping(int sockfd, int seq, int pid) {
    // Dynamic buffer allocation for safety with large -s sizes
    size_t packet_size = sizeof(struct icmp_header) + flags.payload_size;
    char *packet = malloc(packet_size);
    if (!packet) {
        perror("malloc");
        exit(1);
    }
    ft_memset(packet, 0, packet_size);

    // 1. Prepare Header
    struct icmp_header *icmp = (struct icmp_header *)packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->id = htons(pid);
    icmp->sequence = htons(seq);

    // 2. Fill Data (Timestamp + Pattern)
    // We must put the timestamp at the beginning of the data area for RTT calc
    if ((size_t)flags.payload_size >= sizeof(struct timeval)) {
        struct timeval *tv = (struct timeval *)(packet + sizeof(struct icmp_header));
        gettimeofday(tv, NULL);
    }

    // Fill the rest with a pattern
    size_t offset = sizeof(struct icmp_header) + sizeof(struct timeval);
    for (size_t i = offset; i < packet_size; i++) {
        packet[i] = (char)('!' + (i % 56)); // Standard ping pattern logic roughly
    }

    // 3. Checksum
    icmp->checksum = checksum(packet, packet_size);

    // 4. Send
    ssize_t ret = sendto(sockfd, packet, packet_size, 0,
                         (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (ret < 0) {
        if (!flags.quiet) printf("ft_ping: sendto: %s\n", strerror(errno));
    } else {
        g_stats.tx_count++;
    }

    free(packet);
}

static void receive_ping(int sockfd, int seq, int pid) {
    char            buffer[4096]; // Buffer for receiving (MTU is usually 1500, so 4096 is safe)
    struct iovec    iov = { .iov_base = buffer, .iov_len = sizeof(buffer) };
    struct msghdr   msg = { .msg_iov = &iov, .msg_iovlen = 1 };
    ssize_t         ret;

    // Use loop to drain socket or ignore wrong packets (like other pings running)
    // We rely on the socket timeout (set in main) to break this loop if packet is lost
    while (1) {
        ret = recvmsg(sockfd, &msg, 0);
        if (ret < 0) {
            // EAGAIN or EWOULDBLOCK means timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return;
            return;
        }

        // Parse IP Header to find ICMP packet
        struct ip *ip = (struct ip *)buffer;
        int ip_hdr_len = ip->ip_hl * 4;

        if (ret < ip_hdr_len + (int)sizeof(struct icmp_header)) continue; // Too short

        struct icmp_header *icmp = (struct icmp_header *)(buffer + ip_hdr_len);

        // Check if it is an ECHO_REPLY and meant for us (ID check)
        if (icmp->type == ICMP_ECHOREPLY && ntohs(icmp->id) == pid) {
            g_stats.rx_count++;

            // Calculate RTT
            double rtt = 0.0;
            // Extract original timestamp if payload was large enough
            if ((size_t)flags.payload_size >= sizeof(struct timeval)) {
                struct timeval *sent_tv = (struct timeval *)(buffer + ip_hdr_len + sizeof(struct icmp_header));
                struct timeval now;
                gettimeofday(&now, NULL);
                double start_ms = (sent_tv->tv_sec * 1000.0) + (sent_tv->tv_usec / 1000.0);
                double end_ms = (now.tv_sec * 1000.0) + (now.tv_usec / 1000.0);
                rtt = end_ms - start_ms;
            }

            update_stats(rtt);

            // Output
            if (!flags.quiet) {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &dest_addr.sin_addr, ip_str, sizeof(ip_str));

                printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                       ret - ip_hdr_len, // Payload size received
                       ip_str,
                       ntohs(icmp->sequence),
                       ip->ip_ttl,
                       rtt);

                if (flags.verbose && ret >= ip_hdr_len + (int)sizeof(struct icmp_header)) {
                     // Check for potential DUP or other verbose info here if needed
                }
            }
            return; // Successfully received our packet
        }
    }
}

int main(int argc, char **argv) {
    // 1. Argument Parsing (Your existing logic)
    parse_args(argc, argv);

    // 2. DNS Resolution
    resolve_destination(target); // Uses your utils.c

    // 3. Socket Creation
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("ft_ping: socket");
        exit(1);
    }

    // 4. Configure Socket Timeout
    // We set the receive timeout to match the interval.
    // This ensures we wake up roughly when it's time to send the next packet if one is lost.
    struct timeval tv_out;
    tv_out.tv_sec = flags.interval_ms / 1000;
    tv_out.tv_usec = (flags.interval_ms % 1000) * 1000;

    // Safety: If interval is huge (e.g. 10s), we might still want a shorter timeout logic,
    // but for ft_ping, blocking for the interval on loss is acceptable behavior.
    // If interval is 0 (flood), we need a minimal timeout to avoid spin-lock.
    if (flags.interval_ms == 0) tv_out.tv_usec = 1000; // 1ms minimal

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        perror("ft_ping: setsockopt");
        close(sockfd);
        exit(1);
    }

    // Set TTL if flag provided
    if (flags.ttl > 0) {
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &flags.ttl, sizeof(flags.ttl)) < 0) {
            perror("ft_ping: setsockopt ttl");
        }
    }

    // 5. Signal Handling
    signal(SIGINT, interrupt_handler);

    // 6. Print Start Info
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest_addr.sin_addr, ip_str, sizeof(ip_str));
    printf("PING %s (%s): %d data bytes\n", target, ip_str, flags.payload_size);

    // 7. Main Loop
    int pid = getpid();
    int seq = 0;
    gettimeofday(&g_stats.t_start, NULL); // Start global timer

    while (!should_stop) {
        double loop_start = get_time_ms();

        send_ping(sockfd, seq, pid);
        receive_ping(sockfd, seq, pid);

        // Check limit count
        if (flags.count > 0 && g_stats.rx_count >= flags.count) break;
        if (flags.count > 0 && seq + 1 >= flags.count) {
             // If we sent enough but didn't receive enough, we might loop once more to wait?
             // Standard behavior: usually stops after sending N packets and waiting for the last.
             // We can break here if we strictly follow transmit count.
             // But usually we wait a bit for the last reply.
             // For simplicity in ft_ping, breaking here is often accepted.
             if (g_stats.tx_count >= flags.count) break;
        }

        seq++;

        // Smart Sleep: Calculate how much time used by send/recv, sleep the remainder
        double loop_end = get_time_ms();
        double elapsed = loop_end - loop_start;
        double time_to_sleep = (double)flags.interval_ms - elapsed;

        if (time_to_sleep > 0 && !should_stop) {
            usleep((useconds_t)(time_to_sleep * 1000.0));
        }
    }

    print_final_stats();
    close(sockfd);
    return (0);
}