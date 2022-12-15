set(SOURCES
    src/qmrom.c
    src/qmrom_log.c
    src/spi_rom_protocol.c
)

add_library(qmrom SHARED ${SOURCES})

target_include_directories(qmrom PUBLIC
    include
)