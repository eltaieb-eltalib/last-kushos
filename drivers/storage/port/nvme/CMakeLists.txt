# Kush 1.1: OpenFabrics Alliance NVMe StorPort miniport (BSD license, see source headers).
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND SOURCE
    nvmeInit.c
    nvmeIo.c
    nvmePwrMgmt.c
    nvmeSnti.c
    nvmeStat.c
    nvmeStd.c
    nvmeWmi.c
    kush_compat.c)
add_library(nvme MODULE ${SOURCE} nvme.rc)
set_module_type(nvme kernelmodedriver)
target_compile_options(nvme PRIVATE -DMINIPORT_REG_DWORD=4 -fms-extensions -Wno-unknown-pragmas -Wno-missing-braces -Wno-unused-function -Wno-pointer-sign)
add_importlibs(nvme storport ntoskrnl hal)
add_cd_file(TARGET nvme DESTINATION reactos/system32/drivers NO_CAB FOR all)
