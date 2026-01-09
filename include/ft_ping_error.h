#ifndef FT_PING_ERROR_H
#define FT_PING_ERROR_H

/*
** Centralized error handling for ft_ping.
**
** Design goals:
** - One place to define user-facing messages
** - Typed error codes (easy to search, refactor, extend)
** - Optional formatted details (like option letter/value)
** - Stable program prefix ("ft_ping" or "ping" depending on message)
*/

typedef enum e_ping_error
{
	PING_ERR_UNKNOWN_OPTION,
	PING_ERR_OPTION_REQUIRES_ARG,
	PING_ERR_INVALID_NUMERIC_ARG,
	PING_ERR_INTERVAL_TOO_SHORT,
	PING_ERR_MULTIPLE_DESTINATIONS,
	PING_ERR_DESTINATION_REQUIRED,
} 	t_ping_error;

/* Prints the error to stderr. Does not exit. */
void	ping_error_print(t_ping_error err, ...);

/* Prints the error to stderr and exits with exit_code. */
void	ping_error_exit(int exit_code, t_ping_error err, ...);

#endif
