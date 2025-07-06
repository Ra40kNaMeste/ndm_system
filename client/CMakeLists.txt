cmake_minimum_required(VERSION 3.12)
project(ndm_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск зависимостей
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNL REQUIRED libnl-3.0 libnl-genl-3.0)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

add_executable(ndm_client ndm_client.cpp)

# Настройка include директорий и линковки
target_include_directories(ndm_client PRIVATE ${LIBNL_INCLUDE_DIRS})
target_link_directories(ndm_client PRIVATE ${LIBNL_LIBRARY_DIRS})
target_link_libraries(ndm_client PRIVATE ${LIBNL_LIBRARIES})

# Настройка JSON библиотеки
target_compile_definitions(ndm_client PRIVATE ${NLOHMANN_JSON_CFLAGS_OTHER})
target_include_directories(ndm_client PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})