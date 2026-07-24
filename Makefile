IDF_PATH   := $(HOME)/esp/esp-idf
IDF_PYTHON := $(HOME)/.espressif/python_env/idf4.4_py3.9_env
IDF_EXPORT := $(IDF_PATH)/export.sh
CERT       := $(HOME)/.platformio/penv/lib/python3.11/site-packages/certifi/cacert.pem
PORT       := $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -1)

SHELL := /bin/bash
.ONESHELL:

shellenv = export IDF_PATH=$(IDF_PATH); \
           export IDF_PYTHON_ENV_PATH=$(IDF_PYTHON); \
           export PATH=$(IDF_PYTHON)/bin:$$PATH; \
           [ -f $(CERT) ] && export SSL_CERT_FILE=$(CERT); \
           . $(IDF_EXPORT) > /dev/null 2>&1

.PHONY: build flash all monitor clean ota

all: build flash

build:
	@$(shellenv) && idf.py build

flash:
	@if [ -z "$(PORT)" ]; then echo "❌ 未检测到设备"; exit 1; fi
	@echo "⚡ 端口: $(PORT)"
	@$(shellenv) && idf.py -p $(PORT) flash

monitor:
	@if [ -z "$(PORT)" ]; then echo "❌ 未检测到设备"; exit 1; fi
	@$(shellenv) && idf.py -p $(PORT) monitor

clean:
	@$(shellenv) && idf.py fullclean

# OTA 推送: make ota 192.168.x.x
OTA_IP := $(word 2,$(MAKECMDGOALS))
# 阻止 make 把 IP 地址当成构建目标报错
$(eval $(OTA_IP):;@:)
.PHONY: $(OTA_IP)

ota: $(OTA_IP)
	@if [ -z "$(OTA_IP)" ]; then echo "❌ Usage: make ota <IP>"; exit 1; fi
	@echo "🔨 Building..."
	@$(shellenv) && idf.py build
	@echo "🧹 清理端口 8000 上的旧 HTTP 服务..."
	@-lsof -ti tcp:8000 | xargs kill 2>/dev/null || true
	@sleep 1
	@echo "📡 Starting HTTP server on port 8000..."
	@cd build && python3 -m http.server 8000 &
	@sleep 2
	@MY_IP=$$(ipconfig getifaddr en0 2>/dev/null || ifconfig | grep 'inet ' | grep -v 127.0.0.1 | awk '{print $$2}' | head -1); \
	echo "🚀 Pushing OTA to $(OTA_IP) from http://$$MY_IP:8000/yds_charger.bin"; \
	echo "{\"ota_url\":\"http://$$MY_IP:8000/yds_charger.bin\"}" | nc -u -w1 $(OTA_IP) 8000; \
	echo "⏳ Waiting for device to reboot (请在串口观察升级日志)..."; \
	sleep 15; \
	echo "✅ 指令已发送，请确认设备是否重启并完成升级"; \
	kill %1 2>/dev/null || true
