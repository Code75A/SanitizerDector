#!/bin/bash

set -e # 遇到错误立即退出

# 配置变量
LLVM_VERSION="llvmorg-20.1.5"
REPO_URL="https://github.com/llvm/llvm-project.git"
INSTALL_PREFIX="/usr/local"
BUILD_DIR="build"
# 启用的项目: clang, 额外工具, lld(链接器), lldb(调试器)
ENABLE_PROJECTS="clang;clang-tools-extra;lld;lldb"
# 目标架构: X86, ARM, AArch64 (根据你的需求调整，减少架构可加快编译)
TARGETS_TO_BUILD="X86;ARM;AArch64"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 1. 检查 root 权限 (安装依赖和最后安装步骤需要)
if [ "$EUID" -ne 0 ]; then 
    log_error "请使用 root 权限运行此脚本 (sudo ./build_llvm.sh)"
    exit 1
fi

log_info "开始 LLVM ${LLVM_VERSION} 源码编译流程..."

# 2. 安装基础构建依赖
# 注意：移除了 libclang-14-dev，避免与正在编译的 LLVM 20 冲突
log_info "正在安装构建依赖..."
apt-get update -y
apt-get install -y \
    build-essential \
    git \
    cmake \
    ninja-build \
    python3 \
    zlib1g-dev \
    libncurses5-dev \
    libxml2-dev \
    libzstd-dev

# 3. 克隆源码 (如果已存在则跳过或更新)
if [ ! -d "llvm-project" ]; then
    log_info "正在克隆 llvm-project 仓库 (这可能需要几分钟)..."
    git clone --depth 1 "${REPO_URL}"
else
    log_warn "llvm-project 目录已存在，跳过克隆。"
fi

cd llvm-project

# 4. 切换到大版本标签
log_info "正在切换到版本标签: ${LLVM_VERSION} ..."
# 先 fetch 确保标签存在 (如果是浅克隆 depth 1，可能需要 fetch tag)
git fetch --tags
if ! git checkout "${LLVM_VERSION}"; then
    log_error "无法 checkout 到 ${LLVM_VERSION}，请检查版本号是否正确。"
    exit 1
fi
log_success "已成功切换到 ${LLVM_VERSION}"

# 5. 创建构建目录
if [ -d "${BUILD_DIR}" ]; then
    log_warn "发现旧的 build 目录，为了防止缓存冲突，建议手动删除或重命名。"
    log_info "脚本将尝试覆盖使用，如果 cmake 报错，请手动 rm -rf build"
fi
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 6. 配置 CMake
log_info "正在运行 CMake 配置 (生成 Ninja 构建文件)..."
# 注意：这里修复了你原命令中的换行和格式问题
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DLLVM_ENABLE_PROJECTS="${ENABLE_PROJECTS}" \
    -DLLVM_TARGETS_TO_BUILD="${TARGETS_TO_BUILD}" \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_USE_LINKER=lld \
    ../llvm

if [ $? -ne 0 ]; then
    log_error "CMake 配置失败！请检查上方的错误日志。"
    exit 1
fi
log_success "CMake 配置完成。"

# 7. 开始编译
log_info "开始编译 (使用 $(nproc) 个线程)... 这将持续很长时间，请耐心等待。"
log_warn "提示：你可以按 Ctrl+C 中断，下次运行脚本时如果在 build 目录重新执行 ninja 会继续编译（但需重新配置）"

ninja -j$(nproc)

if [ $? -ne 0 ]; then
    log_error "编译失败 (Ninja)！"
    exit 1
fi
log_success "编译完成！"

# 8. 验证编译出的版本 (在安装前)
log_info "验证编译出的 clang 版本..."
./bin/clang --version

# 9. 安装到系统
log_info "正在执行 sudo ninja install 到 ${INSTALL_PREFIX} ..."
ninja install

if [ $? -ne 0 ]; then
    log_error "安装失败！"
    exit 1
fi

log_success "=========================================="
log_success "LLVM ${LLVM_VERSION} 安装成功!"
log_success "安装路径: ${INSTALL_PREFIX}"
log_success "=========================================="
log_info "请运行以下命令验证系统全局版本："
echo "   ${INSTALL_PREFIX}/bin/clang --version"
log_info "如果需要将新版本的 clang 设为默认，请考虑更新 PATH 或使用 update-alternatives。"

apt-get install libclang-14-dev

./build.sh

