#!/bin/bash
set -e

# ========================================
#  yds_charger 一键编译 + 烧写脚本
#  ESP-IDF v4.4.8 + ESP32-C3
# ========================================

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
IDF_PATH="$HOME/esp/esp-idf"
IDF_PYTHON_ENV="$HOME/.espressif/python_env/idf4.4_py3.9_env"
CERT_FILE="$HOME/.platformio/penv/lib/python3.11/site-packages/certifi/cacert.pem"

# ---- 自动检测串口 ----
detect_port() {
    local port
    port=$(ls /dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1)
    echo "$port"
}

PORT="$(detect_port)"
if [ -z "$PORT" ]; then
    echo "❌ 未检测到串口设备，请确认设备已连接"
    echo "   可用端口:"
    ls /dev/cu.* 2>/dev/null | grep -iE "usb|serial|wchusb|cp210" || echo "   (无)"
    exit 1
fi
echo "🔌 检测到设备: $PORT"

# ---- 检查 IDF 环境 ----
if [ ! -d "$IDF_PATH" ]; then
    echo "❌ 未找到 ESP-IDF: $IDF_PATH"
    exit 1
fi

# ---- 激活环境 ----
echo "🔧 激活 ESP-IDF 环境..."
export IDF_PATH="$IDF_PATH"
export IDF_PYTHON_ENV_PATH="$IDF_PYTHON_ENV"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
[ -f "$CERT_FILE" ] && export SSL_CERT_FILE="$CERT_FILE"

. "$IDF_PATH/export.sh" > /dev/null 2>&1

cd "$PROJECT_DIR"

# ---- 选择操作 ----
case "${1:-all}" in
    build)
        echo "🔨 编译中..."
        idf.py build
        echo "✅ 编译完成"
        ;;
    flash)
        echo "⚡ 烧写中..."
        idf.py -p "$PORT" flash
        echo "✅ 烧写完成"
        ;;
    monitor)
        echo "📟 打开串口监视器 (Ctrl+] 退出)..."
        idf.py -p "$PORT" monitor
        ;;
    all|*)
        echo "🔨 编译中..."
        idf.py build
        echo "✅ 编译完成"
        echo ""
        echo "⚡ 烧写中 (端口: $PORT)..."
        idf.py -p "$PORT" flash
        echo ""
        echo "✅ 全部完成！"
        ;;
esac
