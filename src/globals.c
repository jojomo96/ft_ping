#include "ft_ping.h"

// definitions (storage) for the globals declared as extern in `ft_ping.h`
t_flags flags = {0};
struct sockaddr_in dest_addr = {0};
volatile sig_atomic_t should_stop = 0;
char *target = NULL;