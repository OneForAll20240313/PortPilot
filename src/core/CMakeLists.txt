# 核心层：@TODO 实现 RingBuffer / AppendBuffer / DoubleBuffer
# 对齐契约：产品/portpilot-design/contracts/buffer.schema.json（ring/append/double + drop-oldest/stop/wrap）
add_library(portpilot_core STATIC
    core.cpp
    serial_device.cpp
)

target_include_directories(portpilot_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)