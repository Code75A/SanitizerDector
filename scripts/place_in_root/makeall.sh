#!/bin/bash
set -e

ROOT_DIR=$(pwd)
TMP_DIR="$ROOT_DIR/bear_temp_json"
MERGED_FILE="$ROOT_DIR/compile_commands.json"

# 准备临时目录
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

echo "🔍 查找所有含 Makefile 的目录..."
mapfile -t MAKE_DIRS < <(find . -type f -iname 'makefile' -exec dirname {} \; | sort -u)

INDEX=0
for DIR in "${MAKE_DIRS[@]}"; do
    echo "🚧 [$((INDEX+1))/${#MAKE_DIRS[@]}] 执行 bear -- make 于目录: $DIR"

    pushd "$DIR" > /dev/null
    bear -- make || echo "⚠️ 警告：$DIR 构建失败，跳过"
    popd > /dev/null

    if [ -f "$DIR/compile_commands.json" ]; then
        cp "$DIR/compile_commands.json" "$TMP_DIR/$INDEX.json"
        ((INDEX++))
    fi
done

echo "🧩 合并 compile_commands.json 文件..."

echo "[" > "$MERGED_FILE"
FIRST=1
for FILE in "$TMP_DIR"/*.json; do
    if [ $FIRST -eq 0 ]; then echo "," >> "$MERGED_FILE"; fi
    jq -c '.[]' "$FILE" >> "$MERGED_FILE"
    FIRST=0
done
echo "]" >> "$MERGED_FILE"

echo "✅ 生成完成：$MERGED_FILE"