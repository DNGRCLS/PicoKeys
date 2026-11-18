cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w CACHE STRING "Board type")  # also runs on RP2040 + ESP8285 clones
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(pico_sdk_import.cmake)

project(picokeys C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Gzip + embed the web page as a C array
set(WEB_SRC ${CMAKE_CURRENT_LIST_DIR}/web/index.html)
set(WEB_HDR ${CMAKE_CURRENT_BINARY_DIR}/generated/web_page.h)
add_custom_command(
  OUTPUT ${WEB_HDR}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/embed_web.py ${WEB_SRC} ${WEB_HDR}
  DEPENDS ${WEB_SRC} ${CMAKE_CURRENT_LIST_DIR}/tools/embed_web.py
  COMMENT "Embedding web page")
add_custom_target(web_page DEPENDS ${WEB_HDR})

add_executable(picokeys
  src/main.c
  src/hal_pico.c
  src/log.c
  src/esp_at.c
  src/cyw_net.c
  src/net.c
  src/dhcpserver.c
  src/web_server.c
  src/usb_hid.c
  src/usb_descriptors.c
  src/settings.c
  src/sha1_b64.c
)
add_dependencies(picokeys web_page)

target_include_directories(picokeys PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_definitions(picokeys PRIVATE
  PICOKEYS_VERSION="2.1"
  PICO_STACK_SIZE=0x1000
)

target_link_libraries(picokeys
  pico_stdlib
  pico_unique_id
  pico_flash
  hardware_flash
  hardware_watchdog
  hardware_uart
  pico_cyw43_arch_lwip_poll
  tinyusb_device
  tinyusb_board
)

# Observe USB setup packets for host-OS fingerprinting
target_link_options(picokeys PRIVATE "-Wl,--wrap=dcd_event_handler")

# USB carries HID + our own log port, so no SDK stdio
pico_enable_stdio_usb(picokeys 0)
pico_enable_stdio_uart(picokeys 0)

pico_set_program_name(picokeys "PicoKeys")
pico_set_program_version(picokeys "2.1")
pico_add_extra_outputs(picokeys)
