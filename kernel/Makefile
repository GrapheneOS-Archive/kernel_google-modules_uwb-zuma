ifneq ($(O),)
# Called from Android build system, use $(O) as kernel build directory.
KDIR := $(O)
endif

export CONFIG_MCPS_FAKE := n

KDIR ?= /lib/modules/$(shell uname -r)/build

PROJECTS_BUILD := $(abspath $(CURDIR)/../projects/linux-build)
ifeq ($(PROJECTS_BUILD),$(abspath $(KDIR)/../../../../..))

# Build in projects/linux-build, use a separated build tree.

O := $(abspath $(KDIR))
M := ../../../kernel
B := $(abspath $O/$M)

$(info *** Building in $B)

default: modules

modules modules_install:
	mkdir -p $B
	ln -srTf include $B/include
	ln -srTf Kbuild $B/Kbuild
	$(MAKE) -C $(KDIR) O=$O M=$M $@

clean help:
	$(MAKE) -C $(KDIR) O=$O M=$M $@

else

# Regular build, build here.

default:
	$(MAKE) -C $(KDIR) M=$(CURDIR)

modules modules_install clean help:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $@

endif
