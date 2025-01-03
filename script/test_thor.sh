#!/bin/bash

# 检查是否提供了 cgroup_dir 文件路径
if [ $# -ne 1 ]; then
  echo "Usage: $0 <cgroup_dir_file>"
  exit 1
fi

LIMO_PATH=/data/memory-offloading

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

cgroup_dir_file="$1"
pids=()

# 定义清理函数以终止所有后台进程
cleanup() {
  echo "Cleaning up..."
  for pid in "${pids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid"
      wait "$pid" 2>/dev/null
    fi
  done
  exit 0
}

# 监听 SIGINT 和 SIGTERM 信号
trap 'cleanup' INT TERM

# 读取 cgroup_dir 文件并启动后台进程
while IFS= read -r line; do
  # 跳过空行
  [[ -z "$line" ]] && continue
  
  # 分割路径和配置文件名称
  IFS=" " read -r cgroup_path conf_filename <<< "$line"
  
  # 确保路径和配置文件名称非空
  if [[ -z "$cgroup_path" || -z "$conf_filename" ]]; then
    echo "Invalid entry: '$line'. Ensure each line contains a cgroup path and a conf file."
    continue
  fi
  
  # 生成日志文件名，假设路径不包含特殊字符
  log_file=$(echo "$cgroup_path" | tr '/' '_')_$TIMESTAMP.log

  echo "$cgroup_path  $conf_filename"
  # 启动 LiMO 进程并将其放到后台
  $LIMO_PATH/LiMO -g "$cgroup_path" -c "$conf_filename" -v >> "$log_file" 2>&1 &
  pid=$!
  pids+=("$pid")

  # 启动采集 memory.stat 和 memory.pressure 的后台循环任务
  (
    while :; do
      echo "========Timestamp: $(date)========" >> "$log_file"
      free -h >> "$log_file" 2>/dev/null
      cat /sys/block/zram0/mm_stat >> "$log_file" 2>/dev/null
      cat "$cgroup_path/memory.current" >> "$log_file" 2>/dev/null
      cat "$cgroup_path/memory.stat" >> "$log_file" 2>/dev/null
      cat "$cgroup_path/memory.pressure" >> "$log_file" 2>/dev/null
      echo "" >> "$log_file"
      sleep 30
    done
  ) &
  pids+=("$!")
done < "$cgroup_dir_file"

# 等待所有后台进程完成
wait
