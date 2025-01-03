#!/bin/bash

# 检查输入参数
if [ "$#" -g 2 ]; then
    echo "Usage: $0 duration"
    exit 1
fi

# 接收 duration 作为参数
duration=$1

# 获取当前时间，用于文件夹和文件名
current_time=$(date +"%Y%m%d%H%M%S")
log_dir="logs_"$current_time"_$2"

# 创建日志目录
mkdir -p "$log_dir"


cleanup() {
    echo "Cleaning up..."
    for i in in `ps -lef | grep wss | grep -v grep | awk '{print $4}'` ; do  kill -9 $i > /dev/null ; done
}

# 获取占用内存最大的前 20 个进程
ps --sort=-%mem -eo pid,comm --no-headers | head -n 20 | while read -r pid comm; do
    # 创建日志文件名
    log_file="${log_dir}/${pid}_${comm}_${current_time}.log"

    # 打印用于确认的文件名
    echo "Creating log for PID: $pid, Process: $comm, Log File: $log_file"

    # 使用后台子进程执行命令，输出到对应日志中
    (
	trap cleanup SIGINT SIGTERM
	cat /proc/$pid/cgroup >> "$log_file"
        while true; do
            ./wss.pl -s 0 $pid $duration >> "$log_file" 2>&1
            # 可以加个休眠，视情况调整
            sleep 1
        done
    ) &
done

trap cleanup SIGINT SIGTERM
# 等待所有子进程
while true
do
	let "j=j+1"
done




echo "Log collection started for all processes."
