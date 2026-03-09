#!/bin/bash

# 使用当前目录作为搜索目录
SEARCH_DIR="$(pwd)"

# 设置输出文件名
OUTPUT_FILE="source_code_dirs.tsv"

# 查找包含 .cpp/.c/.h 文件的所有目录，去重后输出
find "$SEARCH_DIR" -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) \
    -exec dirname {} \; | sort -u | while read -r dir; do
    echo "$(realpath "$dir")"
done > "$OUTPUT_FILE"

echo "完成：包含源码的目录已输出至 $OUTPUT_FILE"