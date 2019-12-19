
add_library(sgm INTERFACE)

target_include_directories(sgm INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> "${CMAKE_CURRENT_BINARY_DIR}")

install(TARGETS sgm
        DESTINATION ${CMAKE_INSTALL_BINDIRs})

add_library(sgm::sgm ALIAS sgm)
