cmake_minimum_required(VERSION 3.25)
project(rawcoro CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# нужно сделать переменную или условие для сборки целей
add_subdirectory(include)
add_subdirectory(tests)
add_subdirectory(bench)
add_subdirectory(example)

#нужно сделать сим линки в build на все таргеты и нужно сделать сим линку для clangd