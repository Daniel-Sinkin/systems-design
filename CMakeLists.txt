cmake_minimum_required(VERSION 3.28)
project(systems-design LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- library (header-only for now) ----------
add_library(ds_sysdes_lib INTERFACE)
target_include_directories(ds_sysdes_lib INTERFACE ${CMAKE_SOURCE_DIR}/src/lib)

# ---------- precompiled header ----------
# We create a thin static lib just to carry the PCH
add_library(ds_sysdes_pch STATIC src/lib/pch.cpp)
target_include_directories(ds_sysdes_pch PUBLIC ${CMAKE_SOURCE_DIR}/src/lib)
target_precompile_headers(ds_sysdes_pch PUBLIC src/lib/pch.hpp)

# ---------- executable ----------
add_executable(main src/app/main.cpp)
target_link_libraries(main PRIVATE ds_sysdes_lib ds_sysdes_pch)
target_precompile_headers(main REUSE_FROM ds_sysdes_pch)

# ---------- warnings ----------
function(set_strict_warnings target_name)
  target_compile_options(${target_name} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wconversion
    -Wfloat-conversion
    -Wsign-conversion
    -Wcast-qual
    -Wcast-align
    -Wformat=2
    -Wundef
    -Wnull-dereference
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wold-style-cast
    -Wzero-as-null-pointer-constant
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wreturn-type
  )
endfunction()

set_strict_warnings(main)
set_strict_warnings(ds_sysdes_pch)
