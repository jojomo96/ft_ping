#include "ft_ping.h"


char *target = NULL;
t_flags flags = {0};
struct sockaddr_in dest_addr;

int send_icmp_request(int sockfd, struct sockaddr_in *dest, int pid, int seq) {
    char packet[sizeof(struct icmp_header) + PAYLOAD_SIZE];
    ft_memset(packet, 0, sizeof(packet));

    // Build ICMP header
    struct icmp_header *icmp = (struct icmp_header *) packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(pid);
    icmp->sequence = htons(seq);

    // Add timestamp payload for RTT measurement
    struct timeval *tv = (struct timeval *) (packet + sizeof(struct icmp_header));
    gettimeofday(tv, NULL);

    // Fill rest of payload with dummy pattern ('A', 'B', ...)
    for (int i = sizeof(struct timeval); i < PAYLOAD_SIZE; i++) {
        packet[sizeof(struct icmp_header) + i] = 'A' + (i % 26);
    }

    // Calculate checksum over entire ICMP packet
    size_t packet_size = sizeof(packet);
    icmp->checksum = checksum(packet, packet_size);

    // Send packet
    ssize_t sent = sendto(sockfd, packet, packet_size, 0,
                          (struct sockaddr *) dest, sizeof(*dest));

    if (sent < 0) {
        perror("sendto failed");
        return -1;
    }

    return 0;
}

void recv_icmp_reply(const int sockfd, const int pid) {
    char recv_buf[1024];
    struct sockaddr_in sender_addr;
    struct iovec iov;
    struct msghdr msg;
    char control[1024];

    ft_memset(&msg, 0, sizeof(msg));
    ft_memset(&sender_addr, 0, sizeof(sender_addr));

    iov.iov_base = recv_buf;
    iov.iov_len = sizeof(recv_buf);

    msg.msg_name = &sender_addr;
    msg.msg_namelen = sizeof(sender_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    const ssize_t bytes_received = recvmsg(sockfd, &msg, 0);
    if (bytes_received < 0) {
        perror("recvmsg failed");
        return;
    }

    // Parse IPv4 header
    struct ip *ip_header = (struct ip *) recv_buf;
    int ip_header_len = ip_header->ip_hl * 4;

    if (bytes_received < ip_header_len + (ssize_t) sizeof(struct icmp_header)) {
        fprintf(stderr, "Packet too short\n");
        return;
    }

    // Parse ICMP header
    struct icmp_header *icmp = (struct icmp_header *) (recv_buf + ip_header_len);

    if (icmp->type != ICMP_ECHOREPLY || ntohs(icmp->id) != pid) {
        return; // Not our packet
    }

    // RTT calculation
    struct timeval *sent_time = (struct timeval *) (recv_buf + ip_header_len + sizeof(struct icmp_header));
    struct timeval now, rtt;
    gettimeofday(&now, NULL);
    timersub(&now, sent_time, &rtt);

    // Convert sender address to readable IP
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender_addr.sin_addr, ip_str, sizeof(ip_str));

    printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%ld.%03d ms\n",
           bytes_received - ip_header_len,
           ip_str,
           ntohs(icmp->sequence),
           ip_header->ip_ttl,
           rtt.tv_sec * 1000L + rtt.tv_usec / 1000,
           rtt.tv_usec % 1000);
}

int main(int argc, char *argv[]) {
    parse_flags_and_target(argc, argv);
    resolve_destination(target);

    int sockfd = create_raw_socket_with_timeout();
    int seq = 1;
    int pid = getpid();

    send_icmp_request(sockfd, &dest_addr, pid, seq);
    recv_icmp_reply(sockfd, pid);

    close(sockfd);
    return 0;
}
