#include "ft_ping_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const char	*ping_error_prefix(t_ping_error err)
{
	return ("ft_ping");
}

static void	ping_error_unknown_option(va_list *ap)
{
	const int opt = va_arg(*ap, int);
	fprintf(stderr, "unknown option '-%c'\n", (char)opt);
}

static void	ping_error_option_requires_arg(va_list *ap)
{
	const int opt = va_arg(*ap, int);
	fprintf(stderr, "option '-%c' requires an argument\n", (char)opt);
}

static void	ping_error_invalid_numeric_arg(va_list *ap)
{
	const char *val = va_arg(*ap, const char *);
	const int opt = va_arg(*ap, int);
	fprintf(stderr, "invalid numeric arg '%s' for -%c\n", val, (char)opt);
}

static void	ping_error_vprint(t_ping_error err, va_list ap)
{
	fprintf(stderr, "%s: ", ping_error_prefix(err));
	switch (err)
	{
		case PING_ERR_UNKNOWN_OPTION:
			ping_error_unknown_option(&ap);
			break;
		case PING_ERR_OPTION_REQUIRES_ARG:
			ping_error_option_requires_arg(&ap);
			break;
		case PING_ERR_INVALID_NUMERIC_ARG:
			ping_error_invalid_numeric_arg(&ap);
			break;
		case PING_ERR_INTERVAL_TOO_SHORT:
			/* Matches the wording you asked for. */
			fprintf(stderr, "-i interval too short: Operation not permitted\n");
			break;
		case PING_ERR_MULTIPLE_DESTINATIONS:
		{
			const char *a = va_arg(ap, const char *);
			const char *b = va_arg(ap, const char *);
			fprintf(stderr, "multiple destinations: '%s' and '%s'\n", a, b);
			break;
		}
		case PING_ERR_DESTINATION_REQUIRED:
			fprintf(stderr, "destination required\n");
			break;
		default:
			fprintf(stderr, "unknown error\n");
			break;
	}
}

void	ping_error_print(t_ping_error err, ...)
{
	va_list ap;

	va_start(ap, err);
	ping_error_vprint(err, ap);
	va_end(ap);
}

void	ping_error_exit(int exit_code, t_ping_error err, ...)
{
	va_list ap;

	va_start(ap, err);
	ping_error_vprint(err, ap);
	va_end(ap);
	exit(exit_code);
}
