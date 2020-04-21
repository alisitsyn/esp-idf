target_include_directories(${COMPONENT_LIB} PUBLIC include)
target_include_directories(${COMPONENT_LIB} PRIVATE private_include)

set(srcs "esp_wake_stub_common.c" "esp_wake_stub_ext.c" "esp_wake_stub_timer.c")
add_prefix(srcs "${CMAKE_CURRENT_LIST_DIR}/" ${srcs})

target_sources(${COMPONENT_LIB} PRIVATE ${srcs})
