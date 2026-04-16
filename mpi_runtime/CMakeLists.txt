cmake_minimum_required(VERSION 3.16)
project(ngs_mpi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Wno-unused-parameter")

find_package(MPI REQUIRED)

add_executable(ngs_mpi src/main.c)
target_include_directories(ngs_mpi PRIVATE ${MPI_C_INCLUDE_PATH})
target_link_libraries(ngs_mpi ${MPI_C_LIBRARIES} m)
target_compile_options(ngs_mpi PRIVATE ${MPI_C_COMPILE_FLAGS})
