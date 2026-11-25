# Kush 1.1: modern software OpenGL (Mesa llvmpipe, OpenGL 4.5) as the default
# OpenGL ICD when the display driver has none (VBE/GOP framebuffer, VMs).
#
# The prebuilt Mesa from mesa-dist-win (MIT, https://github.com/pal1000/mesa-dist-win)
# is downloaded at configure time and checked against KUSH_MESA_SHA256; only
# libgallium_wgl.dll is taken (x64 -> system32, x86 -> SysWOW64). opengl32.dll
# stays ReactOS': it loads libgallium_wgl.dll as an ICD through
# HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\MesaLLVMpipe,
# chosen by HKLM\SOFTWARE\Kush\OpenGL "DefaultICD" (opengl32/icdload.c).
# Without network access the build goes on without it (old Mesa 2.6, GL 1.1).
#
# Turn off with -DKUSH_MESA_LLVMPIPE=OFF.

option(KUSH_MESA_LLVMPIPE "Ship Mesa llvmpipe (software OpenGL 4.5) as the default ICD" ON)

if(NOT KUSH_MESA_LLVMPIPE OR SYSWOW64_BUILD OR NOT ARCH STREQUAL "amd64")
    return()
endif()

set(KUSH_MESA_VERSION "26.3.1")
set(KUSH_MESA_URL "https://github.com/pal1000/mesa-dist-win/releases/download/${KUSH_MESA_VERSION}/mesa3d-${KUSH_MESA_VERSION}-release-mingw.7z")
set(KUSH_MESA_SHA256 "c7d8c39f43a248a06debb5523d39facb58d0e029e0334ed49bfd23273e4809cf")

set(_mesa_dir ${CMAKE_CURRENT_BINARY_DIR}/mesa-${KUSH_MESA_VERSION})
set(_mesa_archive ${_mesa_dir}/mesa3d-mingw.7z)

if(NOT EXISTS ${_mesa_dir}/x64/libgallium_wgl.dll)
    file(MAKE_DIRECTORY ${_mesa_dir})
    message(STATUS "Kush: downloading Mesa ${KUSH_MESA_VERSION} (llvmpipe)")
    # No EXPECTED_HASH here: a failed download must only warn, and some
    # CMake builds (RosBE's) have no HTTPS at all ("Unsupported protocol").
    file(DOWNLOAD ${KUSH_MESA_URL} ${_mesa_archive} STATUS _mesa_status TLS_VERIFY ON)
    list(GET _mesa_status 0 _mesa_code)
    if(NOT _mesa_code EQUAL 0)
        list(GET _mesa_status 1 _mesa_msg)
        message(STATUS "Kush: CMake could not download Mesa (${_mesa_msg}), trying curl/wget")
        file(REMOVE ${_mesa_archive})
        find_program(KUSH_CURL curl)
        find_program(KUSH_WGET wget)
        if(KUSH_CURL)
            execute_process(COMMAND ${KUSH_CURL} -fsSL --retry 3 -o ${_mesa_archive} ${KUSH_MESA_URL}
                            RESULT_VARIABLE _mesa_code)
        elseif(KUSH_WGET)
            execute_process(COMMAND ${KUSH_WGET} -q -O ${_mesa_archive} ${KUSH_MESA_URL}
                            RESULT_VARIABLE _mesa_code)
        endif()
    endif()
    set(_mesa_hash "")
    if(EXISTS ${_mesa_archive})
        file(SHA256 ${_mesa_archive} _mesa_hash)
    endif()
    if(NOT _mesa_hash STREQUAL KUSH_MESA_SHA256)
        message(WARNING "Kush: Mesa download failed or its SHA256 does not match; building without llvmpipe.")
        file(REMOVE ${_mesa_archive})
        return()
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${_mesa_archive}
                            x64/libgallium_wgl.dll x86/libgallium_wgl.dll
                    WORKING_DIRECTORY ${_mesa_dir}
                    RESULT_VARIABLE _mesa_x)
    if(NOT EXISTS ${_mesa_dir}/x64/libgallium_wgl.dll)
        # Older CMake (libarchive) may not read every 7z filter; use 7-Zip.
        find_program(KUSH_7Z NAMES 7z 7za 7zr)
        if(KUSH_7Z)
            execute_process(COMMAND ${KUSH_7Z} x -y ${_mesa_archive}
                                    x64/libgallium_wgl.dll x86/libgallium_wgl.dll
                            WORKING_DIRECTORY ${_mesa_dir}
                            OUTPUT_QUIET
                            RESULT_VARIABLE _mesa_x)
        endif()
    endif()
    file(REMOVE ${_mesa_archive})
    if(NOT EXISTS ${_mesa_dir}/x64/libgallium_wgl.dll)
        message(WARNING "Kush: could not extract Mesa; building without llvmpipe.")
        return()
    endif()
endif()

add_cd_file(FILE ${_mesa_dir}/x64/libgallium_wgl.dll DESTINATION reactos/system32 NO_WOW64 FOR all)
if(BUILD_WOW64_ENABLED AND EXISTS ${_mesa_dir}/x86/libgallium_wgl.dll)
    add_cd_file(FILE ${_mesa_dir}/x86/libgallium_wgl.dll DESTINATION reactos/SysWOW64 NO_WOW64 FOR all)
endif()
add_cd_file(FILE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.mesa.txt DESTINATION reactos/system32 NO_WOW64 FOR all)
