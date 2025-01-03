# Define the compiler
CC = gcc
#CC = aarch64-linux-gnu-gcc

# Define compilation options, such as debug information and warnings
CFLAGS = -Wall -g
CFLAGS = -Wall -g -static

# Define linker options
LDFLAGS = -static -lpthread -lm

# Define the target executable name
TARGET = LiMO

# Define all source files
SRCS = LiMO.c cgroup_op.c parse_config.c logger.c

# Define all header files
HDRS = logger.h cgroup_op.h parse_config.h

# Automatically generate all object files (.o files)
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link object files to generate the executable
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Rule to compile .c files into .o files
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean

