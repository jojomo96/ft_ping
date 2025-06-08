#include "libft/ft_getopt.h"
#include "libft/libft.h"
#include "ft_ping.h"

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/ip.h>



char *target = NULL;
t_flags flags = {0};

struct sockaddr_in dest_addr;

void parse_flags_and_target(int argc, char **argv) {
    int c;
    while ((c = ft_getopt(argc, argv, "v?")) != -1) {
        switch (c) {
            case 'v':
                flags.verbose = 1;
                break;
            case '?':
                printf("Usage: ft_ping [-v] <hostname/IP>\n");
                exit(0);
            default:
                fprintf(stderr, "Usage: ft_ping [-v] <hostname/IP>\n");
                exit(1);
        }
    }

    for (int i = ft_optind; i < argc; ++i) {
        printf("non-option arg: %s\n", argv[i]);
    }

    if (ft_optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        exit(1);
    }

    target = argv[ft_optind];
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

int create_raw_socket() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    return sockfd;
}

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }

    if (len == 1) {
        sum += *(unsigned char *) buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

size_t create_icmp_packet(char *packet, int pid, int seq) {
    struct icmp_header *icmp = (struct icmp_header *) packet;

    // Fill the ICMP header
    icmp->type = ICMP_ECHO; // Echo request
    icmp->code = 0; // No code for echo request
    icmp->checksum = 0; // Initially set to 0 for checksum calculation
    icmp->id = htons(pid); // Set the process ID
    icmp->sequence = htons(seq); // Set the sequence number

    // Fill the payload with current time + padding
    struct timeval *tv = (struct timeval *) (packet + sizeof(struct icmp_header));
    gettimeofday(tv, NULL);

    // Fill the rest of the payload with a pattern
    ft_memset(packet + sizeof(struct icmp_header) + sizeof(struct timeval), 0x42,
              PAYLOAD_SIZE - sizeof(struct timeval));

    // Calculate checksum over entire packet
    size_t total_size = sizeof(struct icmp_header) + PAYLOAD_SIZE;
    icmp->checksum = checksum(packet, total_size);

    return total_size;
}

void set_socket_timeout(int sockfd) {
    struct timeval timeout;
    timeout.tv_sec = 1; // Set timeout to 1 second
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        exit(1);
    }
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

    if (bytes_received < ip_header_len + (ssize_t)sizeof(struct icmp_header)) {
        fprintf(stderr, "Packet too short\n");
        return;
    }

    // Parse ICMP header
    struct icmp_header *icmp = (struct icmp_header *)(recv_buf + ip_header_len);

    if (icmp->type != ICMP_ECHOREPLY || ntohs(icmp->id) != pid) {
        return; // Not our packet
    }

    // RTT calculation
    struct timeval *sent_time = (struct timeval *)(recv_buf + ip_header_len + sizeof(struct icmp_header));
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
    char packet[sizeof(struct icmp_header) + PAYLOAD_SIZE];

    int sockfd;
    int seq = 1;


    resolve_destination(target);
    sockfd = create_raw_socket();
    set_socket_timeout(sockfd);

    const size_t len = create_icmp_packet(packet, getpid(), seq);
    if (sendto(sockfd, packet, len, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        perror("sendto failed");
        close(sockfd);
        exit(1);
    }


    recv_icmp_reply(sockfd, getpid());

    close(sockfd);
    return 0;
}
