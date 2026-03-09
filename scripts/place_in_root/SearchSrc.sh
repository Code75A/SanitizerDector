#!/bin/bash

# 使用当前目录作为搜索目录
SEARCH_DIR="$(pwd)"

# 设置输出文件名（输出到当前目录）
OUTPUT_FILE="source_file_paths.tsv"

# 输出表头
echo -e "Type\tPath" > "$OUTPUT_FILE"

# 查找并输出 .cpp 和 .c 文件
find "$SEARCH_DIR" -type f \( -name "*.cpp" -o -name "*.c" \) | while read -r file; do
    echo -e "Source\t$(realpath "$file")" >> "$OUTPUT_FILE"
done

# 查找并输出 .h 文件
find "$SEARCH_DIR" -type f -name "*.h" | while read -r file; do
    echo -e "Header\t$(realpath "$file")" >> "$OUTPUT_FILE"
done

echo "完成：路径表已输出至 $OUTPUT_FILE"