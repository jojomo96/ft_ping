#include "ft_ping.h"
#include "ping_options.h"

void ft_usage(const int exit_code) {
    fprintf(stdout, "Usage: ft_ping [options] <destination>\n\nOptions:\n");
    for (int i = 0; g_options[i].handler; i++) {
        char buf[64];
        int len = 0;
        const t_ping_opt *opt = &g_options[i];

        /* Format short opt */
        if (opt->short_name)
            len += sprintf(buf + len, "  -%c", opt->short_name);
        else
            len += sprintf(buf + len, "    ");

        /* Format separator */
        len += sprintf(buf + len, "%s", (opt->short_name && opt->long_name) ? ", " : "  ");

        /* Format long opt */
        if (opt->long_name)
            len += sprintf(buf + len, "--%s", opt->long_name);

        /* Format argument label */
        if (opt->type == ARG_REQ && opt->arg_label)
            len += sprintf(buf + len, " <%s>", opt->arg_label);

        fprintf(stdout, "%-35s %s\n", buf, opt->desc);
    }
    exit(exit_code);
}