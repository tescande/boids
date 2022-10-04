# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)
project(boids_sim)

set(BOIDS boids)

find_package(PkgConfig REQUIRED)

add_executable(${BOIDS}
	boids.c
	gui.c
	swarm.c
)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

target_compile_options(${BOIDS} PRIVATE -Wall -O3 ${GTK3_CFLAGS_OTHER})
target_include_directories(${BOIDS} PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_libraries(${BOIDS} PRIVATE ${GTK3_LIBRARIES} -lm)

install(TARGETS ${BOIDS} DESTINATION bin)
