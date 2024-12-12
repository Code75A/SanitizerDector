#!/bin/bash

# 定义输入文件和目标文件夹
input_file="/home/sd/SanitizerDector/mutants/mutants241210/result.txt"
target_dir="/home/sd/SanitizerDector/data/2bugs2reduce"

# 创建目标文件夹（如果不存在）
mkdir -p "$target_dir"

# 从 result.txt 提取路径并复制文件
grep -oP 'found a bug in \K(/[\w/._-]+\.c)' "$input_file" | while read -r file_path; do
    if [ -f "$file_path" ]; then
        cp "$file_path" "$target_dir"
        echo "已复制文件: $file_path 到 $target_dir"
    else
        echo "文件不存在: $file_path"
    fi
done
