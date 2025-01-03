#!/bin/bash

# 检查是否以 root 用户运行
if [ "$EUID" -ne 0 ]; then
  echo "请以 root 权限运行该脚本。"
  exit 1
fi

# 检查是否提供了间隔时间参数
if [ "$#" -ge 3 ]; then
  echo "用法: $0 <间隔时间（秒）>"
  exit 1
fi

INTERVAL=$1

# 获取当前时间，用于日志文件名
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# 临时目录，用于存放日志文件
LOG_DIR="./process_logs_"$TIMESTAMP"_$2"
mkdir -p "$LOG_DIR"

# 获取占用内存最大的前 20 个进程的 PID 和名称
TOP_PROCESSES=$(ps -e -o pid=,rss=,comm= --sort=-rss | head -n 20)

# 创建一个关联数组，将 PID 映射到日志文件名
declare -A PID_TO_LOGFILE
declare -A PID_TO_INITIAL_REFERENCED

while read -r PID RSS COMM; do
  # 跳过空行
  if [ -z "$PID" ]; then
    continue
  fi

  # 检查进程是否存在，以防止进程在此期间退出
  if [ ! -d "/proc/$PID" ]; then
    echo "$(date +"%Y-%m-%d %H:%M:%S") 进程 PID: $PID 不存在，跳过"
    continue
  fi

  # 去除进程名称中的特殊字符
  SAFE_COMM=$(echo "$COMM" | tr -cd '[:alnum:]_')

  LOGFILE="${LOG_DIR}/${PID}_${SAFE_COMM}_${TIMESTAMP}.log"
  PID_TO_LOGFILE["$PID"]="$LOGFILE"

# 获取原始 /proc/PID/smaps_rollup 信息
  if [ -r "/proc/$PID/smaps_rollup" ]; then
    SMAPS_ORIG=$(cat "/proc/$PID/smaps_rollup")
  else
    echo "无法读取 /proc/$PID/smaps_rollup，进程 PID: $PID"
    continue
  fi

  # 将 1 写入 /proc/PID/clear_refs
  if [ -w "/proc/$PID/clear_refs" ]; then
    echo 1 > "/proc/$PID/clear_refs"
  else
    echo "无法写入 /proc/$PID/clear_refs，进程 PID: $PID"
    continue
  fi

  # 获取初始 /proc/PID/smaps_rollup 信息
  if [ -r "/proc/$PID/smaps_rollup" ]; then
    SMAPS_ROLLUP=$(cat "/proc/$PID/smaps_rollup")
  else
    echo "无法读取 /proc/$PID/smaps_rollup，进程 PID: $PID"
    continue
  fi

  # 从 smaps_rollup 中提取初始 Referenced 值（KB）
  INITIAL_REFERENCED=$(echo "$SMAPS_ROLLUP" | awk '/^Referenced:/ {print $2}')
  PID_TO_INITIAL_REFERENCED["$PID"]="$INITIAL_REFERENCED"

  # 提取初始 RSS、PSS 值（KB）
  INITIAL_RSS=$(echo "$SMAPS_ROLLUP" | awk '/^Rss:/ {print $2}')
  INITIAL_PSS=$(echo "$SMAPS_ROLLUP" | awk '/^Pss:/ {print $2}')

  # 将值转换为 MB，并保留两位小数
  INITIAL_RSS_MB=$(awk "BEGIN {printf \"%.2f\", $INITIAL_RSS/1024}")
  INITIAL_PSS_MB=$(awk "BEGIN {printf \"%.2f\", $INITIAL_PSS/1024}")
  INITIAL_REFERENCED_MB=$(awk "BEGIN {printf \"%.2f\", $INITIAL_REFERENCED/1024}")

  # 获取当前时间
  CURRENT_TIME=$(date +"%Y-%m-%d %H:%M:%S")

  # 写入初始信息到日志文件
  {
    echo "初始时间：$CURRENT_TIME"
    echo "进程名称：$COMM"
    echo "进程 PID：$PID"
    echo "$SMAPS_ORIG"
    echo "初始 RSS：$INITIAL_RSS_MB MB"
    echo "初始 PSS：$INITIAL_PSS_MB MB"
    echo "初始 Referenced：$INITIAL_REFERENCED_MB MB"
    echo "---------------------------"
    echo "初始 /proc/$PID/smaps_rollup 内容："
    echo "$SMAPS_ROLLUP"
    echo "============================================="
  } > "$LOGFILE"

done <<< "$TOP_PROCESSES"

echo "日志文件已创建，日志目录：$LOG_DIR"
echo "开始监控进程内存使用情况，间隔：$INTERVAL 秒"

# 定义一个函数，获取进程的 memory cgroup 路径
get_mem_cgroup_path() {
  local pid=$1
  local cgroup_path

  # 检查 cgroup v1 或 v2
  if [ -f "/proc/$pid/cgroup" ]; then
    cgroup_path=$(awk -F: '/memory|0/ {print $3}' "/proc/$pid/cgroup" | head -n1)
    if [ -z "$cgroup_path" ]; then
      cgroup_path="未知"
    fi
  else
    cgroup_path="未知"
  fi

  echo "$cgroup_path"
}

# 无限循环，按间隔时间采集数据
while true; do
  for PID in "${!PID_TO_LOGFILE[@]}"; do
    LOGFILE="${PID_TO_LOGFILE[$PID]}"

    # 检查进程是否存在
    if [ ! -d "/proc/$PID" ]; then
      echo "$(date +"%Y-%m-%d %H:%M:%S") 进程 PID: $PID 已不存在，跳过" | tee -a "$LOGFILE"
      continue
    fi

    # 获取进程名称
    COMM=$(cat "/proc/$PID/comm" 2>/dev/null)
    if [ -z "$COMM" ]; then
      echo "无法获取进程名称，PID: $PID" | tee -a "$LOGFILE"
      continue
    fi

    # 获取进程所在的 memory cgroup
    MEM_CGROUP=$(get_mem_cgroup_path "$PID")

    # 获取 /proc/PID/smaps_rollup 信息
    if [ -r "/proc/$PID/smaps_rollup" ]; then
      SMAPS_ROLLUP=$(cat "/proc/$PID/smaps_rollup")
    else
      echo "无法读取 /proc/$PID/smaps_rollup，PID: $PID" | tee -a "$LOGFILE"
      continue
    fi

    # 从 smaps_rollup 中提取 RSS、PSS、Referenced
    RSS=$(echo "$SMAPS_ROLLUP" | awk '/^Rss:/ {print $2}')
    PSS=$(echo "$SMAPS_ROLLUP" | awk '/^Pss:/ {print $2}')
    REFERENCED=$(echo "$SMAPS_ROLLUP" | awk '/^Referenced:/ {print $2}')

    # 获取初始 Referenced
    INITIAL_REFERENCED="${PID_TO_INITIAL_REFERENCED[$PID]}"
    if [ -z "$INITIAL_REFERENCED" ]; then
      echo "无法获取初始 Referenced 值，PID: $PID" | tee -a "$LOGFILE"
      continue
    fi

    # 计算 Referenced 差值
    REFERENCED_DELTA=$((REFERENCED - INITIAL_REFERENCED))

    # 将值转换为 MB，并保留两位小数
    RSS_MB=$(awk "BEGIN {printf \"%.2f\", $RSS/1024}")
    PSS_MB=$(awk "BEGIN {printf \"%.2f\", $PSS/1024}")
    REFERENCED_DELTA_MB=$(awk "BEGIN {printf \"%.2f\", $REFERENCED_DELTA/1024}")

    # 获取当前时间
    CURRENT_TIME=$(date +"%Y-%m-%d %H:%M:%S")

    # 写入日志文件
    {
      echo "时间：$CURRENT_TIME"
      echo "进程名称：$COMM"
      echo "进程 PID：$PID"
      echo "Memory cgroup 路径：$MEM_CGROUP"
      echo "RSS：$RSS_MB MB"
      echo "PSS：$PSS_MB MB"
      echo "Referenced 差值：$REFERENCED_DELTA_MB MB"
      echo "---------------------------"
      echo "/proc/$PID/smaps_rollup 内容："
      echo "$SMAPS_ROLLUP"
      echo "============================================="
    } >> "$LOGFILE"

  done

  sleep "$INTERVAL"
done
