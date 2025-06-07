#include <stdio.h>

int ft_getopt(int argc, char ** argv, char * str);

char * opt_arg;

int opt_index;

int main(int argc, char **argv) {
    int c;
    while ((c = ft_getopt(argc, argv, "abo:")) != -1) {
        switch (c) {
            case 'a': puts("saw -a");          break;
            case 'b': puts("saw -b");          break;
            case 'o': printf("-o value = %s\n", opt_arg); break;
            case '?':                         /* unknown switch   */
                /* tiny_getopt already printed a message */
                return 1;
            case ':':                         /* missing argument */
                /* message already printed                        */
                return 1;
        }
    }

    /* Remaining arguments start at tiny_optind */
    for (int i = opt_index; i < argc; ++i)
        printf("non-option argument: %s\n", argv[i]);

    return 0;
}
