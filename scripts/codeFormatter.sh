#!/bin/bash

#TARGET_DIR="/path/to/your/source/folder"  # ←←← 请在此处填写你的目标文件夹路径
TARGET_DIR="/root/sseq/example/asterisk"


# Uncrustify 配置文件路径（根据你的实际情况调整）
CONFIG_FILE="/root/sseq/braces.cfg"



# 检查目标目录是否存在
if [ ! -d "$TARGET_DIR" ]; then
    echo "error the target dir is not existed : $TARGET_DIR"
    exit 1
fi

# 进入目标目录（避免路径拼接问题）
cd "$TARGET_DIR" || { echo "can not cd into dir: $TARGET_DIR"; exit 1; }

# 查找所有 .c 文件并逐个处理
shopt -s nullglob  # 避免没有 .c 文件时出错
c_files=(*.c)

if [ ${#c_files[@]} -eq 0 ]; then
    echo "did not find any .c files in dir: $TARGET_DIR"
    exit 0
fi

echo "find ${#c_files[@]} .c file ，start formatting..."

for file in "${c_files[@]}"; do
    echo "formatting : $file"
    uncrustify -c "$CONFIG_FILE" -f "$file" -o "$file"
done
