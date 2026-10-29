#===============================================================================
# @brief    SLE static relay sample cmake file
#===============================================================================

set(SLE_UART_SAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../sle_uart)
set(OLED_SAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../peripheral/helloworld_oled)

set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_relay.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rescue_route.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server/sle_uart_server_adv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server/sle_uart_server.c
)

set(HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server
    ${SLE_UART_SAMPLE_DIR}
)

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT OR "CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT=y" IN_LIST DEFINES)
    list(APPEND SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/ground_model.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ground_ui.c
        ${CMAKE_CURRENT_SOURCE_DIR}/aht20.c
        ${OLED_SAMPLE_DIR}/ssd1306.c
        ${OLED_SAMPLE_DIR}/ssd1306_fonts.c
    )
    list(APPEND HEADER_LIST
        ${OLED_SAMPLE_DIR}
    )
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM OR "CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM=y" IN_LIST DEFINES)
    list(APPEND SOURCES_LIST ${SLE_UART_SAMPLE_DIR}/max30102.c)
endif()

set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${HEADER_LIST} PARENT_SCOPE)
