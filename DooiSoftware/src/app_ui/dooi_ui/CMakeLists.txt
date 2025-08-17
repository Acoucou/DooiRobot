zephyr_include_directories(./)

add_subdirectory(digital_clock)
add_subdirectory(music_ui)
add_subdirectory(emoji_gif)
add_subdirectory(uncanny_eyes)
add_subdirectory(camera)
add_subdirectory(lucky_cat)
add_subdirectory(info)
add_subdirectory(page_select)
add_subdirectory(wakeup_ack)

zephyr_library_sources(
    screen.c
)



