#include <stdio.h>
#include <string.h>

int opt_index = 1;
int opt_opt = 0;
int opt_err = 1;
char *opt_arg = NULL;

int ft_getopt(int argc, char *const argv[], const char *opt_string) {
    static int opt_pos = 1;
    char opt;
    const char *spec;

    if (opt_index >= argc ||
        argv[opt_index][0] != '-' ||
        argv[opt_index][1] == '\0')
        return -1;

    if (strcmp(argv[opt_index], "--") == 0) {
        ++opt_index;
        return -1;
    }

    opt = argv[opt_index][opt_pos];
    spec = strchr(opt_string, opt);

    if (!spec) {
        opt_opt = opt;
        if (opt_err) {
            fprintf(stderr, "ft_getopt: unknown option '-%c'\n", opt);
        }
        goto advance_char;
        return '?';
    }

    if (spec[1] == ':') {
        if (argv[opt_index][opt_pos + 1] != '\0') {
            opt_arg = &argv[opt_index][opt_pos + 1];

            if (*opt_arg == '=')                 /* skip leading '=' */
                ++opt_arg;

            goto advance_argv;
        }

        if (opt_index + 1 < argc) {
            opt_arg = argv[opt_index + 1];
            opt_index += 2;
            opt_pos = 1;
            return opt;
        }

        opt_opt = opt;
        if (opt_err)
            fprintf(stderr, "ft_getopt: option '-%c' requires an argument\n", opt);
        ++opt_index;
        opt_pos = 1;
        return ':';
    }

    opt_arg = NULL;

advance_char:
    if (argv[opt_index][++opt_pos] == '\0') {
advance_argv:
        ++opt_index;
        opt_pos = 1;
    }
    return opt;
}

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