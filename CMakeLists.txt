cmake_minimum_required(VERSION 3.31)
project(ft_ping C)

# Set the C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add include directory for your headers
include_directories(include)

# Add the external library
add_subdirectory(external/libft)

# Gather your source files
file(GLOB_RECURSE SOURCES "src/*.c")

# Define the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link with the external libft library
target_link_libraries(${PROJECT_NAME} PRIVATE libft)

# Optionally reinforce include path
target_include_directories(${PROJECT_NAME} PRIVATE include)
