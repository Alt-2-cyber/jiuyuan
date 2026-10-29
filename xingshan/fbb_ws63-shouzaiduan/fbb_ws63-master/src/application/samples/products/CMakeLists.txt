#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
#===============================================================================


if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_UART OR "CONFIG_SAMPLE_SUPPORT_SLE_UART=y" IN_LIST DEFINES)
    add_subdirectory_if_exist(sle_uart)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_UART OR "CONFIG_SAMPLE_SUPPORT_BLE_UART=y" IN_LIST DEFINES)
    add_subdirectory_if_exist(ble_uart)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_GETAWAY OR "CONFIG_SAMPLE_SUPPORT_SLE_GETAWAY=y" IN_LIST DEFINES)
    add_subdirectory_if_exist(sle_gateway)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_RELAY OR "CONFIG_SAMPLE_SUPPORT_SLE_RELAY=y" IN_LIST DEFINES)
    add_subdirectory_if_exist(sle_relay)
endif()


set(SOURCES "${SOURCES}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" PARENT_SCOPE)
set(LIBS "${LIBS}" PARENT_SCOPE)
