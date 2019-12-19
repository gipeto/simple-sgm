
add_executable(simple-sgm simple-sgm.cpp utils.h)
target_link_libraries(simple-sgm sgm::sgm stb::stb)

install(TARGETS simple-sgm
        DESTINATION ${CMAKE_INSTALL_BINDIRs})
