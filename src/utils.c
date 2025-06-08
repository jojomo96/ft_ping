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

int create_raw_socket_with_timeout() {
    const int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    set_socket_timeout(sockfd);
    return sockfd;
}