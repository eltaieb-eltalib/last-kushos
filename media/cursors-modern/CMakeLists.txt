# Exact upstream files and their source/permission notice are installed together.
file(GLOB MODERN_CURSOR_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cur"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ani"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.txt")
list(REMOVE_ITEM MODERN_CURSOR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
foreach(f ${MODERN_CURSOR_FILES})
    get_filename_component(name "${f}" NAME)
    add_cd_file(FILE "${f}" DESTINATION reactos/Cursors NAME_ON_CD "KushModern-${name}" FOR all)
    add_cd_file(FILE "${f}" DESTINATION reactos/Cursors NAME_ON_CD "KushModern-${name}" NO_CAB FOR bootcd)
endforeach()
