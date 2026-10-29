#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
#===============================================================================
set(OLED_SAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../peripheral/helloworld_oled)
set(MQTT_SOURCE_DIR ${ROOT_DIR}/open_source/mqtt/paho.mqtt.c/src)

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER OR "CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER=y" IN_LIST DEFINES)
set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server/sle_uart_server_adv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server/sle_uart_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rescue_cloud.c
    ${CMAKE_CURRENT_SOURCE_DIR}/max30102.c
    ${OLED_SAMPLE_DIR}/ssd1306_fonts.c
    ${OLED_SAMPLE_DIR}/ssd1306.c
)
set(HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server
    ${MQTT_SOURCE_DIR}
    ${OLED_SAMPLE_DIR}
)
elseif(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT OR "CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT=y" IN_LIST DEFINES)
set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_client/sle_uart_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rescue_cloud.c
)
set(HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_client
    ${MQTT_SOURCE_DIR}
)
endif()

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
