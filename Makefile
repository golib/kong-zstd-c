GOOS ?= $(shell go env GOOS)
GOARCH ?= $(shell go env GOARCH)
GOOS_GOARCH := $(GOOS)_$(GOARCH)
GOOS_GOARCH_NATIVE := $(shell go env GOHOSTOS)_$(shell go env GOHOSTARCH)
LIBZSTD_NAME := libzstd_$(GOOS_GOARCH).so
ZSTD_VERSION ?= master
MOREFLAGS ?= -fpic

.PHONY: libzstd.so

clean-libzstd.a:
	cd zstd && $(MAKE) clean

libzstd.a: clean-libzstd.a
ifeq ($(GOOS_GOARCH),$(GOOS_GOARCH_NATIVE))
	cd zstd/lib && ZSTD_LEGACY_SUPPORT=0 MOREFLAGS=$(MOREFLAGS) $(MAKE) clean libzstd.a
	mv zstd/lib/libzstd.a lib/libzstd_$(GOOS_GOARCH).a
else
	ifeq ($(GOOS_GOARCH),linux_arm)
		cd zstd/lib && CC=arm-linux-gnueabi-gcc ZSTD_LEGACY_SUPPORT=0 MOREFLAGS=$(MOREFLAGS) $(MAKE) clean libzstd.a
		mv zstd/lib/libzstd.a lib/libzstd_$(GOOS_GOARCH).a
	endif
	ifeq ($(GOOS_GOARCH),linux_arm64)
		cd zstd/lib && CC=aarch64-linux-gnu-gcc ZSTD_LEGACY_SUPPORT=0 MOREFLAGS=$(MOREFLAGS) $(MAKE) clean libzstd.a
		mv zstd/lib/libzstd.a lib/libzstd_$(GOOS_GOARCH).a
	endif
endif

clean-libzstd.so:
	rm -f lib/libzstd_$(GOOS_GOARCH).a lib/base64_$(GOOS_GOARCH).o lib/kong_$(GOOS_GOARCH).o lib/$(LIBZSTD_NAME)

libzstd.so: clean-libzstd.so libzstd.a
	gcc -I./zstd/lib -Wall -Werror -fpic -o lib/base64_$(GOOS_GOARCH).o -c base64.c
	gcc -I./zstd/lib -Wall -Werror -fpic -o lib/kong_$(GOOS_GOARCH).o -c kong_zstd.c
	gcc -I./lib -shared -o lib/$(LIBZSTD_NAME) lib/base64_$(GOOS_GOARCH).o lib/kong_$(GOOS_GOARCH).o lib/libzstd_$(GOOS_GOARCH).a

update-zstd:
	rm -rf zstd-tmp
	git clone --branch $(ZSTD_VERSION) --depth 1 https://github.com/Facebook/zstd zstd-tmp
	rm -rf zstd-tmp/.git
	rm -rf zstd
	mv zstd-tmp zstd
	$(MAKE) clean libzstd.a
	cp zstd/lib/zstd.h .
	cp zstd/lib/dictBuilder/zdict.h .
	cp zstd/lib/common/zstd_errors.h .

test-luajit:
	luajit luajit/zstd.lua
