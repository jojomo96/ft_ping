#include "libft/ft_getopt.h"
#include "libft/libft.h"
#include "ft_ping.h"

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/time.h>


char *target = NULL;
t_flags flags = {0};

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

struct sockaddr_in get_dest_addr(const char *hostname) {
    struct addrinfo hints, *res;
    ft_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Error resolving hostname: %s\n", hostname);
        exit(1);
    }
    const struct sockaddr_in dest_addr = *(struct sockaddr_in *) res->ai_addr;
    freeaddrinfo(res);
    return dest_addr;
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

int main(int argc, char *argv[]) {
    struct sockaddr_in dest_addr;
    int sockfd;
    char packet[sizeof(struct icmp_header) + PAYLOAD_SIZE];

    parse_flags_and_target(argc, argv);

    dest_addr = get_dest_addr(target);
    sockfd = create_raw_socket();

    const size_t len = create_icmp_packet(packet, getpid(), 1);
    if (sendto(sockfd, packet, len, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        perror("sendto failed");
        close(sockfd);
        exit(1);
    }


    // Receive the ICMP reply (optional verbose output)
    char recv_buf[1024];
    socklen_t addr_len = sizeof(dest_addr);
    const ssize_t bytes_received = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&dest_addr, &addr_len);
    if (bytes_received < 0) {
        perror("Receive failed");
        exit(1);
    }

    if (flags.verbose) {
        printf("Received %ld bytes from %s: icmp_seq=%d ttl=%d\n",
               bytes_received, target, 1, 64);  // For simplicity, assuming TTL=64 here
    }

    close(sockfd);
    return 0;
}
