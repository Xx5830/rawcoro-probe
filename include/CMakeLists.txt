message(STATUS "Include is configurable")

add_library(asy INTERFACE)

target_include_directories(
    asy SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

message(STATUS "Include is ready")