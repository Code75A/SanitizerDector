#!/bin/bash
set -e

echo "📁 当前目录：$(pwd)"
echo "🔍 开始遍历所有 Makefile 所在目录..."

find $MutantHome -type f -iname 'makefile' | while read -r makefile; do
    dir=$(dirname "$makefile")
    echo "🚧 进入 $dir 执行 bear -- make"
    (cd "$dir" && bear -- make)
done

#find . -type f -iname 'makefile' -execdir bash -c 'echo "==> Building in $(pwd)"; bear -- make' \;