# AngelScript 2.34.0 (zlib licence) — vendored as a tarball, like the other
# third-party dependencies. Upstream ships a CMake project under
# projects/cmake, but it is written as a standalone build with install/export
# and framework handling, so the target is defined here instead.
#
# Endianness: the Unicorn Overlord bytecode is stored big-endian (saved on the
# Switch). AngelScript's bytecode reader (as_restore.cpp) swaps multi-byte
# reads on little-endian hosts, so the .asb files load on either kind of host;
# no build flag is required.

include(FetchContent)

FetchContent_Declare(angelscript
    URL "${CMAKE_CURRENT_LIST_DIR}/angelscript-2.34.0.tar.xz"
    URL_HASH MD5=4397566c114f848b89a4602468f8489b
)
FetchContent_MakeAvailable(angelscript)

find_package(Threads REQUIRED)

set(_as_dir "${angelscript_SOURCE_DIR}")

set(_as_sources
    ${_as_dir}/source/as_atomic.cpp
    ${_as_dir}/source/as_builder.cpp
    ${_as_dir}/source/as_bytecode.cpp
    ${_as_dir}/source/as_callfunc.cpp
    ${_as_dir}/source/as_callfunc_x86.cpp
    ${_as_dir}/source/as_callfunc_x64_gcc.cpp
    ${_as_dir}/source/as_callfunc_x64_msvc.cpp
    ${_as_dir}/source/as_callfunc_x64_mingw.cpp
    ${_as_dir}/source/as_callfunc_mips.cpp
    ${_as_dir}/source/as_compiler.cpp
    ${_as_dir}/source/as_configgroup.cpp
    ${_as_dir}/source/as_context.cpp
    ${_as_dir}/source/as_datatype.cpp
    ${_as_dir}/source/as_gc.cpp
    ${_as_dir}/source/as_generic.cpp
    ${_as_dir}/source/as_globalproperty.cpp
    ${_as_dir}/source/as_memory.cpp
    ${_as_dir}/source/as_module.cpp
    ${_as_dir}/source/as_objecttype.cpp
    ${_as_dir}/source/as_outputbuffer.cpp
    ${_as_dir}/source/as_parser.cpp
    ${_as_dir}/source/as_restore.cpp
    ${_as_dir}/source/as_scriptcode.cpp
    ${_as_dir}/source/as_scriptengine.cpp
    ${_as_dir}/source/as_scriptfunction.cpp
    ${_as_dir}/source/as_scriptnode.cpp
    ${_as_dir}/source/as_scriptobject.cpp
    ${_as_dir}/source/as_string.cpp
    ${_as_dir}/source/as_string_util.cpp
    ${_as_dir}/source/as_thread.cpp
    ${_as_dir}/source/as_tokenizer.cpp
    ${_as_dir}/source/as_typeinfo.cpp
    ${_as_dir}/source/as_variablescope.cpp
)

# The MIPS/x86/x64 calling-convention backends guard themselves with the
# preprocessor, so they are always listed. The assembler backends are only
# added where the toolchain can actually build them.
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(ASM_MASM)
    list(APPEND _as_sources ${_as_dir}/source/as_callfunc_x64_msvc_asm.asm)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    enable_language(ASM)
    list(APPEND _as_sources
        ${_as_dir}/source/as_callfunc_arm.cpp
        ${_as_dir}/source/as_callfunc_arm_gcc.S)
    set_source_files_properties(${_as_dir}/source/as_callfunc_arm_gcc.S
        PROPERTIES COMPILE_OPTIONS "-Wa,-mimplicit-it=always")
endif()

add_library(angelscript STATIC ${_as_sources})
target_include_directories(angelscript PUBLIC ${_as_dir}/include)
target_compile_definitions(angelscript PRIVATE ANGLESCRIPT_EXPORT _LIB)
target_link_libraries(angelscript PUBLIC Threads::Threads)

if(MSVC)
    target_compile_definitions(angelscript PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(angelscript PRIVATE /MP)
else()
    set_target_properties(angelscript PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Standard add-ons the scripts rely on (string and array value types).
add_library(angelscript_addon STATIC
    ${_as_dir}/add_on/scriptarray/scriptarray.cpp
    ${_as_dir}/add_on/scriptstdstring/scriptstdstring.cpp
    ${_as_dir}/add_on/scriptstdstring/scriptstdstring_utils.cpp
)
target_include_directories(angelscript_addon PUBLIC ${_as_dir}/add_on)
target_link_libraries(angelscript_addon PUBLIC angelscript)

# AngelScript 2.34 predates C++20; build it as C++17 regardless of the project
# default so the legacy headers (e.g. as_symboltable.h) stay warning-free.
foreach(_as_target angelscript angelscript_addon)
    set_target_properties(${_as_target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endforeach()
