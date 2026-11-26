cmake_minimum_required(VERSION 3.25)
project(Starfall LANGUAGES CXX RC)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Starfall requires MSVC x86")
endif()

# These are the same game translation units as build.cmd. Lua is compiled
# as C++ in both builds so its exceptions unwind through the same runtime.
file(GLOB _game CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/modules/ActSp/*.cpp")
file(GLOB _lua CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/references/lua/*.c")
set(_yrpp "${CMAKE_CURRENT_SOURCE_DIR}/references/Phobos/YRpp")
# Both variants of YRpp describe the same game binary, but some inline
# functions have different bodies (including EventClass and HouseClass).
# Keep these game types private to the core so MSVC cannot coalesce their
# C++ COMDATs with the host. Only the existing C ABI crosses this boundary.
file(GLOB_RECURSE _yrpp_headers CONFIGURE_DEPENDS "${_yrpp}/*.h")
set(_game_types REGISTERS ExtendedRegister LimitedRegister StackRegister)
set(_type_declaration "^(class|struct)[ \t]+(NOVTABLE[ \t]+)?([A-Z][A-Za-z0-9_]*(Class|Struct)|[A-Za-z0-9_]*Surface)")
foreach(_header IN LISTS _yrpp_headers)
    file(STRINGS "${_header}" _declarations REGEX "${_type_declaration}([ \t:;{]|$)")
    foreach(_declaration IN LISTS _declarations)
        string(REGEX MATCH "${_type_declaration}([ \t:;{]|$)" _match "${_declaration}")
        list(APPEND _game_types "${CMAKE_MATCH_3}")
    endforeach()
endforeach()
# The two YRpp variants declare different surface implementations. In
# particular BSurface's inline destructor otherwise shares a linker symbol.
foreach(_surface Surface XSurface BSurface DSurface)
    if(NOT _surface IN_LIST _game_types)
        message(FATAL_ERROR "Missing embedded YRpp type isolation for ${_surface}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _game_types)
list(SORT _game_types)
set(_private_names "#pragma once\n")
foreach(_name IN LISTS _game_types)
    string(APPEND _private_names "#define ${_name} StarfallYrpp_${_name}\n")
endforeach()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/starfall_yrpp_names.h" CONTENT "${_private_names}" @ONLY)
set_source_files_properties(${_lua} PROPERTIES LANGUAGE CXX)
set_source_files_properties(${_game} PROPERTIES COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_game_hooks.h")
add_library(starfall_embedded OBJECT ${_game} ${_lua} assets/research/ui.rc)
set_target_properties(starfall_embedded PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES
    MSVC_RUNTIME_LIBRARY MultiThreaded INTERPROCEDURAL_OPTIMIZATION OFF)
target_include_directories(starfall_embedded PRIVATE "${_yrpp}" "${CMAKE_CURRENT_BINARY_DIR}"
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_definitions(starfall_embedded PRIVATE STARFALL_EMBEDDED SYR_VER=2 NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(starfall_embedded PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")

# rc.exe resolves RCDATA paths relative to its include paths. Tell Ninja
# and MSBuild to rebuild the resource when the contents of an asset change.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/assets/research/ui.rc" _resource_lines REGEX "RCDATA")
foreach(_line IN LISTS _resource_lines)
    string(REGEX MATCH "\"([^\"]+)\"" _file "${_line}")
    string(REPLACE "\\\\" "/" _path "${CMAKE_MATCH_1}")
    list(APPEND _assets "${CMAKE_CURRENT_SOURCE_DIR}/${_path}")
endforeach()
set_property(SOURCE assets/research/ui.rc APPEND PROPERTY OBJECT_DEPENDS "${_assets}")
