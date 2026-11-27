# Tests de host del firmware del ESP32: corren en la Mac, sin placa.
#   make -C firmware_esp32c3/test
# Igual que firmware/Makefile: cada test se compila tal cual —tiene que PASAR— y
# una vez por mutante —una version rota a proposito que tiene que FALLAR—.
HOST_CC ?= cc
XCODE_SDK := /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
ifneq ($(wildcard $(XCODE_SDK)),)
HOST_ENV = SDKROOT=$(XCODE_SDK)
endif
CFLAGS = -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -lm

MUTANTES = -DMIC_MARGEN_REL_DB=-100.0f -DMIC_PISO_ABS_DBFS=-300.0f

test:
	@mkdir -p build
	@$(HOST_ENV) $(HOST_CC) test_mic_dsp.c $(CFLAGS) -o build/mic_dsp && ./build/mic_dsp
	@for m in $(MUTANTES); do \
	  $(HOST_ENV) $(HOST_CC) $$m test_mic_dsp.c $(CFLAGS) -o build/mutante || exit 1; \
	  if ./build/mutante >/dev/null; then echo "✗ el mutante $$m PASA: el test no lo detecta"; exit 1; \
	  else echo "  mutante $$m: detectado"; fi; \
	done
.PHONY: test
