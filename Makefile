NAME        = ft_ping
CC          = cc
CFLAGS      = -Wall -Wextra -Werror -std=gnu17 -g
INCLUDES    = -I./include -I./external/libft/include

SRC_DIR     = src
OBJ_DIR     = obj
LIBFT_DIR   = external/libft
LIBFT       = $(LIBFT_DIR)/libft.a

# Source files
SRCS        = $(wildcard $(SRC_DIR)/*.c)
OBJS        = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

CAP_NEED    := cap_net_raw+ep
CAP_STAMP   := $(OBJ_DIR)/.cap_net_raw

all: $(NAME) $(CAP_STAMP)

$(NAME): $(LIBFT) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBFT) -o $(NAME) -lm
	@rm -f $(CAP_STAMP)

$(CAP_STAMP): $(NAME)
	@mkdir -p $(OBJ_DIR)
	@cur="$$(getcap -n ./$(NAME) 2>/dev/null | awk '{print $$2}')" ; \
	if [ "$$cur" != "$(CAP_NEED)" ]; then \
		echo "Setting CAP_NET_RAW on ./$(NAME)"; \
		sudo setcap $(CAP_NEED) ./$(NAME); \
	fi
	@touch $@

# Compile objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(DEPS)

# Build libft (calls Makefile in external/libft)
$(LIBFT):
	@if [ ! -f $(LIBFT_DIR)/Makefile ]; then \
		echo "libft submodule not initialized, initializing..."; \
		if command -v git >/dev/null 2>&1; then \
			git submodule update --init --recursive; \
		else \
			echo "Error: git not found, cannot initialize submodule."; \
			exit 1; \
		fi; \
	fi
	@$(MAKE) -C $(LIBFT_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@if [ -f $(LIBFT_DIR)/Makefile ]; then \
		$(MAKE) -C $(LIBFT_DIR) clean; \
	fi

fclean: clean
	@rm -f $(NAME)
	@if [ -f $(LIBFT_DIR)/Makefile ]; then \
		$(MAKE) -C $(LIBFT_DIR) fclean; \
	fi

re: fclean all

.PHONY: all clean fclean re
