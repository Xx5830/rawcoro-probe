message(STATUS "Example is configurable")

add_executable(example echo-server.cpp)

target_link_libraries(example PUBLIC asy)

message(STATUS "Example is ready")