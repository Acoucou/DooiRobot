set(ICO_LIB_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/lib/libico.a)

add_library(ico STATIC IMPORTED GLOBAL)
set_target_properties(ico PROPERTIES IMPORTED_LOCATION ${ICO_LIB_PATH})
target_include_directories(zephyr_interface INTERFACE ./inc)
target_link_libraries(app PUBLIC ico)

