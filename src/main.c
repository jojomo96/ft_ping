#include "libft/ft_getopt.h"
#include "header.h"

int main(int argc, char *argv[])
{
    int c;
    int verbose = 0;

    while ((c = ft_getopt(argc, argv, "v?")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case '?':
                printf("Usage: ft_ping [-v] <hostname/IP>\n");
                exit(0);
                break;
            default:
                fprintf(stderr, "Usage: ft_ping [-v] <hostname/IP>\n");
                exit(1);
        }
    }

    for (int i = ft_optind; i < argc; ++i)
        printf("non-option arg: %s\n", argv[i]);

    return 0;
}
