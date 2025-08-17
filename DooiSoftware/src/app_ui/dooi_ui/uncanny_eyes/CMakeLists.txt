
zephyr_include_directories(./)
zephyr_include_directories(./eyes)

add_library(eyes STATIC IMPORTED)
set_target_properties(eyes PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/libeyes.a
)
target_link_libraries(app PUBLIC eyes)


