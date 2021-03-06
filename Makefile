# SPDX-License-Identifier: MIT
CC = gcc
CFLAGS = -Wall -O3 `pkg-config --cflags gtk+-3.0`
LINKFLAGS = `pkg-config --libs gtk+-3.0` -lm
INCS = boids.h vector.h
SRCS = boids.c swarm.c gui.c
OBJS = $(SRCS:%.c=%.o)
TARGET = boids

default: all

all: $(TARGET)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LINKFLAGS)

clean:
	rm -f $(TARGET) *.o

boids.o: $(INCS)
swarm.o: $(INCS)
gui.o: $(INCS)
