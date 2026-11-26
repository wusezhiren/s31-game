set(S31_SRCS
    app_main.c
    s31_board_idf.c
    s31_font_cjk_16.c
    s31_serial_upload.c
    ../src/platform.c
    ../src/rom_loader.c
    ../src/video.c
    ../src/audio.c
    ../src/input.c
    ../src/gamepad.c
    ../src/nes.c
    ../src/nes_video.c
    ../src/game_catalog.c
    ../src/gb.c
    ../src/sms.c
    ../src/pce_adapter.c
    ../src/scale.c
    ../third_party/gnuboy/gnuboy.c
    ../third_party/gnuboy/cpu.c
    ../third_party/gnuboy/hw.c
    ../third_party/gnuboy/lcd.c
    ../third_party/gnuboy/sound.c
    ../third_party/smsplus/crc32.c
    ../third_party/smsplus/system.c
    ../third_party/smsplus/loadrom.c
    ../third_party/smsplus/sms.c
    ../third_party/smsplus/memz80.c
    ../third_party/smsplus/pio.c
    ../third_party/smsplus/vdp.c
    ../third_party/smsplus/render.c
    ../third_party/smsplus/tms.c
    ../third_party/smsplus/cpu/z80.c
    ../third_party/smsplus/sound/sound.c
    ../third_party/smsplus/sound/sn76489.c
    ../third_party/pce-go/crc32.c
    ../third_party/pce-go/pce-go.c
    ../third_party/pce-go/pce.c
    ../third_party/pce-go/gfx.c
    ../third_party/pce-go/h6280.c
    ../third_party/pce-go/psg.c
    ../third_party/nes/core/nes.c
    ../third_party/nes/core/nes_default.c
    ../third_party/nes/core/nes_rom.c
    ../third_party/nes/core/nes_cpu.c
    ../third_party/nes/core/nes_apu.c
    ../third_party/nes/core/nes_mapper.c
    ../third_party/nes/core/nes_ppu.c
    ../third_party/nes/core/nes_mapper/nes_mapper0.c
    ../third_party/nes/core/nes_mapper/nes_mapper1.c
    ../third_party/nes/core/nes_mapper/nes_mapper2.c
    ../third_party/nes/core/nes_mapper/nes_mapper3.c
    ../third_party/nes/core/nes_mapper/nes_mapper7.c
    ../third_party/nes/core/nes_mapper/nes_mapper94.c
    ../third_party/nes/core/nes_mapper/nes_mapper117.c
    ../third_party/nes/core/nes_mapper/nes_mapper180.c
    s31_bt_gamepad_stub.c
    s31_usb_gamepad.c
    s31_betop_dual.c
)
file(GLOB INFONES_MAPPER_SRCS ../third_party/nes/core/nes_mapper/infones/*.c)
list(APPEND S31_SRCS ../third_party/nes/core/nes_mapper/nes_mapper_infones.c ${INFONES_MAPPER_SRCS})

idf_component_register(
    SRCS ${S31_SRCS}
    INCLUDE_DIRS "../include" "." "../third_party/nes/inc" "../third_party/nes/port" "../third_party/gnuboy" "../third_party/smsplus" "../third_party/pce-go"
    REQUIRES bsp_ues31s043h800v480c_u esp_lcd esp_lvgl_port esp_codec_dev esp_driver_ppa esp_mm
             fatfs sdmmc driver esp_timer esp_hid bt nvs_flash usb esp_hw_support
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_NES_MAPPER_INFONES_ENABLE=1 CONFIG_NES_MAPPER_INFONES_ALL=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
set_source_files_properties(
    ../third_party/gnuboy/gnuboy.c
    ../third_party/gnuboy/cpu.c
    PROPERTIES COMPILE_OPTIONS "-Wno-error=unterminated-string-initialization;-Wno-error=maybe-uninitialized")
set_source_files_properties(
    ../third_party/smsplus/render.c
    ../third_party/smsplus/tms.c
    ../third_party/smsplus/cpu/z80.c
    PROPERTIES COMPILE_OPTIONS "-Wno-error=unused-variable;-Wno-error=maybe-uninitialized;-Wno-error=stringop-overflow")
set_source_files_properties(
    ../third_party/pce-go/pce-go.c
    ../third_party/pce-go/pce.c
    ../third_party/pce-go/gfx.c
    ../third_party/pce-go/h6280.c
    ../third_party/pce-go/psg.c
    PROPERTIES COMPILE_OPTIONS "-Wno-error=sequence-point;-Wno-error=unused-variable;-Wno-error=format")

# ESP-IDF exposes the Bluedroid GAP headers through the API subdirectory; the
# esp_hid component itself does not export that path to application components.
target_include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/bt/host/bluedroid/api/include/api"
    "$ENV{IDF_PATH}/components/bt/include/esp32s31/include"
    "$ENV{IDF_PATH}/components/bt/porting_btdm/controller/btdm_common/include"
    "$ENV{IDF_PATH}/components/bt/porting_btdm/controller/bredr/include"
    "$ENV{IDF_PATH}/components/bt/porting_btdm/controller/ble/include")
