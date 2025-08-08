#!/bin/bash

# 监控的 cgroup v2 目录数组
# 注意这些路径需按实际环境调整，比如 /sys/fs/cgroup/group1
# orin A
CGROUPSA=(
    "/sys/fs/cgroup/system.slice/planning_group"
    "/sys/fs/cgroup/system.slice/datarec_group"
    "/sys/fs/cgroup/system.slice/env_container_group"
    "/sys/fs/cgroup/system.slice/vla_parking_group"
    "/sys/fs/cgroup/system.slice/realtime_container_group"
    "/sys/fs/cgroup/system.slice/vla_arch_group"
)


# 日志文件存放位置（启动时间命名）
LOG_DIR="/data/logs"
mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/cgroup_log_$(date '+%Y%m%d_%H%M%S').log"

echo "日志输出到: $LOG_FILE"

# 3. 主循环
while true; do
    {
        echo "========== $(date '+%Y-%m-%d %H:%M:%S') =========="
        for cg in "${CGROUPSA[@]}"; do
            echo "-- cg path: $cg $(date '+%Y-%m-%d %H:%M:%S') --"

            # 打印 memory.stat
            if [ -f "$cg/memory.stat" ]; then
                echo "[memory.stat]"
                cat "$cg/memory.stat"
            else
                echo "memory.stat 文件不存在"
            fi

            # 打印 memory current/min/max/high/low
            for memfile in memory.current memory.min memory.max memory.high memory.low; do
                if [ -f "$cg/$memfile" ]; then
                    echo "[-- $memfile --] $(cat "$cg/$memfile")"
                else
                    echo "$memfile 文件不存在"
                fi
            done

            # 打印 cgroup.procs 及进程名
            if [ -f "$cg/cgroup.procs" ]; then
                echo "[cgroup.procs]"
                while read -r pid; do
                    if [ -d "/proc/$pid" ]; then
                        pname=$(tr -d '\0' < "/proc/$pid/comm")
                        echo "PID: $pid  CMD: $pname"
                    else
                        echo "PID: $pid  (进程已退出)"
                    fi
                done < "$cg/cgroup.procs"
            else
                echo "cgroup.procs 文件不存在"
            fi

            echo
        done

        # 打印系统信息
        echo "---------- free -h ----------"
        free -h
        echo "------ /sys/block/zram0/stat ------"
        if [ -f /sys/block/zram0/stat ]; then
            cat /sys/block/zram0/stat
        else
            echo "/sys/block/zram0/stat 文件不存在"
        fi
        echo "------ /sys/block/zram0/mm_stat ------"
        if [ -f /sys/block/zram0/mm_stat ]; then
            cat /sys/block/zram0/mm_stat
        else
            echo "/sys/block/zram0/mm_stat 文件不存在"
        fi
        echo "------ /proc/vmstat ------"
        if [ -f /proc/vmstat ]; then
            cat /proc/vmstat
        else
            echo "/proc/vmstat 文件不存在"
        fi

        echo
    } >> "$LOG_FILE"

    sleep 30
done

