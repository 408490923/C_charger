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

.PHONY: build flash all monitor clean

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
