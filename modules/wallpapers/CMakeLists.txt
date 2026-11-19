
file(GLOB WALLPAPER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.bmp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.txt"
)

list(REMOVE_ITEM WALLPAPER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")

foreach(f ${WALLPAPER_FILES})
    add_cd_file(FILE "${f}" DESTINATION reactos/Web/Wallpaper FOR all)
endforeach()

# Also place the wallpapers as loose files on the boot CD, so the LiveCD
# desktop can use them: without NO_CAB they only land inside reactos.cab
# (which the installer extracts), and the LiveCD never extracts that cab,
# so the registry wallpaper path would point at a missing file and the
# desktop falls back to a solid colour.
foreach(f ${WALLPAPER_FILES})
    add_cd_file(FILE "${f}" DESTINATION reactos/Web/Wallpaper NO_CAB FOR bootcd)
endforeach()
