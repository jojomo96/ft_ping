#include <stdio.h>
#include "libft/ft_getopt.h"

int main(int argc, char *argv[])
{
    int c;

    while ((c = ft_getopt(argc, argv, "abo:c::")) != -1) {
        switch (c) {
            case 'a':
                puts("saw -a");
                break;
            case 'b':
                puts("saw -b");
                break;
            case 'o':
                printf("-o value = %s\n", ft_optarg);
                break;
            case 'c':
                if (ft_optarg)
                    printf("-c optional arg = %s\n", ft_optarg);
                else
                    puts("-c with no arg");
                break;
            case '?':
                return 1;
            case ':':
                return 1;
        }
    }

    for (int i = ft_optind; i < argc; ++i)
        printf("non-option arg: %s\n", argv[i]);

    return 0;
}
