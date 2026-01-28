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

all: $(NAME) cap

$(NAME): $(LIBFT) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBFT) -o $(NAME) -lm

# Grant capability so ft_ping can open raw sockets without full root.
# This requires sudo once during `make`. After that, running ./ft_ping works unprivileged.
cap: $(NAME)
	@printf "Setting CAP_NET_RAW on ./%s\n" "$(NAME)"
	@sudo setcap cap_net_raw+ep ./$(NAME)
	@getcap ./$(NAME) || true

# Compile objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build libft (calls Makefile in external/libft)
$(LIBFT):
	@if [ ! -f $(LIBFT_DIR)/Makefile ]; then \
		echo "Initializing libft submodule..."; \
		git submodule update --init external/libft || { \
			echo "Error: Failed to initialize submodule. Please check your git configuration."; \
			exit 1; \
		}; \
	fi
	@make -C $(LIBFT_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@if [ -f $(LIBFT_DIR)/Makefile ]; then \
		make -C $(LIBFT_DIR) clean; \
	fi

fclean: clean
	@rm -f $(NAME)
	@if [ -f $(LIBFT_DIR)/Makefile ]; then \
		make -C $(LIBFT_DIR) fclean; \
	fi

re: fclean all

.PHONY: all clean fclean re cap
