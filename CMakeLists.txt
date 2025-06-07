cmake_minimum_required(VERSION 3.31)
project(ft_ping C)

set(CMAKE_C_STANDARD 17)

add_executable(ft_ping main.c
)

add_subdirectory(external/libft)
target_link_libraries(ft_ping PRIVATE libft)