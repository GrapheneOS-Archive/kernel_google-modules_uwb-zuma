ccflags-y := -I$(srctree)/$(src)/libqmrom/include -Werror

obj-m := qm35.o

qm35-y := \
	qm35-spi.o \
	qm35_rb.o \
	qmrom_spi.o \
	libqmrom/src/qmrom.o \
	libqmrom/src/qmrom_log.o \
	libqmrom/src/spi_rom_protocol.o \
	hsspi.o \
	hsspi_uci.o \
	hsspi_log.o \
	hsspi_coredump.o \
	debug.o \
	hsspi_test.o

qm35-$(CONFIG_EVENT_TRACING) += qm35-trace.o
CFLAGS_qm35-trace.o = -I$(srctree)/$(src)
