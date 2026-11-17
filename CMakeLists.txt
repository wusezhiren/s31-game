cmake_minimum_required(VERSION 3.16)
if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(s31_game)
else()
    project(s31_game C)
    set(CMAKE_C_STANDARD 11)
    add_library(s31_host
        src/platform.c src/rom_loader.c src/video.c src/audio.c src/input.c src/gamepad.c src/nes.c src/nes_video.c src/game_catalog.c src/gb.c src/sms.c src/pce_adapter.c src/scale.c
        third_party/gnuboy/gnuboy.c third_party/gnuboy/cpu.c third_party/gnuboy/hw.c third_party/gnuboy/lcd.c third_party/gnuboy/sound.c
        third_party/smsplus/crc32.c third_party/smsplus/system.c third_party/smsplus/loadrom.c
        third_party/smsplus/sms.c third_party/smsplus/memz80.c third_party/smsplus/pio.c
        third_party/smsplus/vdp.c third_party/smsplus/render.c third_party/smsplus/tms.c
        third_party/smsplus/cpu/z80.c third_party/smsplus/sound/sound.c third_party/smsplus/sound/sn76489.c
        third_party/pce-go/crc32.c third_party/pce-go/pce-go.c third_party/pce-go/pce.c
        third_party/pce-go/gfx.c third_party/pce-go/h6280.c third_party/pce-go/psg.c
        third_party/nes/core/nes.c
        third_party/nes/core/nes_default.c
        third_party/nes/core/nes_rom.c
        third_party/nes/core/nes_cpu.c
        third_party/nes/core/nes_mapper.c
        third_party/nes/core/nes_ppu.c
        third_party/nes/core/nes_mapper/nes_mapper0.c
        third_party/nes/core/nes_mapper/nes_mapper1.c
        third_party/nes/core/nes_mapper/nes_mapper2.c
        third_party/nes/core/nes_mapper/nes_mapper3.c
        third_party/nes/core/nes_mapper/nes_mapper7.c
        third_party/nes/core/nes_mapper/nes_mapper94.c
        third_party/nes/core/nes_mapper/nes_mapper117.c
        third_party/nes/core/nes_mapper/nes_mapper180.c)
    target_include_directories(s31_host PUBLIC include third_party/nes/inc third_party/nes/port third_party/gnuboy third_party/smsplus third_party/pce-go)
    file(GLOB INFONES_MAPPER_SRCS third_party/nes/core/nes_mapper/infones/*.c)
    target_sources(s31_host PRIVATE third_party/nes/core/nes_mapper/nes_mapper_infones.c ${INFONES_MAPPER_SRCS})
    target_compile_definitions(s31_host PRIVATE CONFIG_NES_MAPPER_INFONES_ENABLE=1 CONFIG_NES_MAPPER_INFONES_ALL=1)
    target_compile_options(s31_host PRIVATE -Wall -Wextra -Wpedantic)
    add_executable(s31_host_test tests/platform_test.c)
    target_link_libraries(s31_host_test PRIVATE s31_host)
    add_executable(s31_nes_video_test tests/nes_video_test.c)
    target_link_libraries(s31_nes_video_test PRIVATE s31_host)
    add_executable(s31_catalog_test tests/game_catalog_test.c)
    target_link_libraries(s31_catalog_test PRIVATE s31_host)
    add_executable(s31_sms_test tests/sms_test.c)
    target_link_libraries(s31_sms_test PRIVATE s31_host)
    add_executable(s31_pce_test tests/pce_test.c)
    target_link_libraries(s31_pce_test PRIVATE s31_host)
    add_executable(s31_gamepad_test tests/gamepad_test.c)
    target_link_libraries(s31_gamepad_test PRIVATE s31_host)
    enable_testing()
    add_test(NAME s31_host_test COMMAND s31_host_test)
    add_test(NAME s31_nes_video_test COMMAND s31_nes_video_test)
    add_test(NAME s31_catalog_test COMMAND s31_catalog_test)
    add_test(NAME s31_sms_test COMMAND s31_sms_test)
    add_test(NAME s31_pce_test COMMAND s31_pce_test)
    add_test(NAME s31_gamepad_test COMMAND s31_gamepad_test)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME s31_serial_upload_tool_test
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/serial_upload_tool_test.py)
    endif()
endif()
