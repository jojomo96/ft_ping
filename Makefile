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

all: $(NAME)

$(NAME): $(LIBFT) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBFT) -o $(NAME) -lm

# Compile objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build libft (calls Makefile in external/libft)
$(LIBFT):
	@make -C $(LIBFT_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@make -C $(LIBFT_DIR) clean

fclean: clean
	@rm -f $(NAME)
	@make -C $(LIBFT_DIR) fclean

re: fclean all

.PHONY: all clean fclean re