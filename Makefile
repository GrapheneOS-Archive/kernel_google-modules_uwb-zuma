KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

M ?= $(shell pwd)
O := $(abspath $(KDIR))
B := $(abspath $O/$M)

$(info *** Building Android in $O/$M )

build:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules

modules_install: build
	$(MAKE) -C $(KERNEL_SRC) M=$(M) INSTALL_MOD_STRIP=1 modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

all: build

.PHONY: all build modules_install clean
