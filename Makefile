# 定义编译器
CC = gcc

# 定义编译选项，例如调试信息和警告
CFLAGS = -Wall -g

# 定义链接选项
LDFLAGS =

# 定义目标可执行文件名
TARGET = senpai

# 定义所有源文件
SRCS = senpai.c cgroup_op.c parse_config.c logger.c

# 定义所有头文件
HDRS = logger.h cgroup_op.h parse_config.h

# 自动生成所有目标文件（.o文件）
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# 编译.c文件生成.o文件的规则
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean

