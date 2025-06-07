/*  ft_getopt.c – short-option parser with optional-argument support
 *
 *  API identical to POSIX getopt():
 *      extern int   opt_index, opt_opt, opt_err;
 *      extern char *opt_arg;
 *      int ft_getopt(int argc, char *const argv[], const char *opt_string);
 *
 *  Recognised opt_string tokens
 *  ----------------------------
 *      'a'      →  flag option
 *      'b:'     →  option takes a **required** argument
 *      'c::'    →  option takes an **optional** argument
 *
 *  Optional-argument rules (same as GNU getopt):
 *      • Attached forms:  -cARG   -c=ARG
 *      • Separate token:  -c ARG          (only if ARG does **not** start with '-')
 *      • No argument:     -c              → opt_arg == NULL
 */
#include <stdio.h>
#include <string.h>
#include "ft_getopt.h"

int opt_index = 1;
int opt_opt = 0;
int opt_err = 1;
char *opt_arg = NULL;

int ft_getopt(int argc, char *const argv[], const char *opt_string) {
    static int opt_pos = 1; /* index inside the current “-xyz” */
    char opt;
    const char *spec;

    /* ---------- no more options? --------------------------------------- */
    if (opt_index >= argc ||
        argv[opt_index][0] != '-' ||
        argv[opt_index][1] == '\0')
        return -1;

    /* "--" terminator */
    if (strcmp(argv[opt_index], "--") == 0) {
        ++opt_index;
        return -1;
    }

    /* ---------- examine current option char ---------------------------- */
    opt = argv[opt_index][opt_pos];
    spec = strchr(opt_string, opt);

    if (!spec) {
        /* unknown option */
        opt_opt = opt;
        if (opt_err)
            fprintf(stderr, "ft_getopt: unknown option '-%c'\n", opt);
        goto advance_char; /* keep scanning this clump       */
        /* NOTREACHED: return '?' */
    }

    /* ------------------------------------------------------------------- */
    if (spec[1] == ':') {
        /* ':'  → argument of some sort   */
        int optional = (spec[2] == ':'); /* '::' → optional argument       */

        /* -- 1. attached argument?  "-oARG" or "-o=ARG" ------------------ */
        if (argv[opt_index][opt_pos + 1] != '\0') {
            opt_arg = &argv[opt_index][opt_pos + 1];
            if (*opt_arg == '=') /* treat "-o=ARG" like "-oARG"    */
                ++opt_arg;
            goto advance_argv; /* consume whole argv element     */
        }

        /* -- 2. separate token?  "-o ARG" -------------------------------- */
        if (opt_index + 1 < argc &&
            (!optional || argv[opt_index + 1][0] != '-')) {
            opt_arg = argv[opt_index + 1];
            opt_index += 2;
            opt_pos = 1;
            return opt;
        }

        /* -- 3. (missing) optional argument ----------------------------- */
        if (optional) {
            /* allowed to be absent */
            opt_arg = NULL;
            goto advance_argv;
        }

        /* -- 4. missing required argument → error ----------------------- */
        opt_opt = opt;
        if (opt_err)
            fprintf(stderr,
                    "ft_getopt: option '-%c' requires an argument\n", opt);
        ++opt_index;
        opt_pos = 1;
        return ':'; /* POSIX “missing arg” signal */
    }

    /* flag option – no argument */
    opt_arg = NULL;

advance_char:
    /* move to next char in current “-abc” clump, else next argv element */
    if (argv[opt_index][++opt_pos] == '\0') {
    advance_argv:
        ++opt_index;
        opt_pos = 1;
    }
    return opt;
}
