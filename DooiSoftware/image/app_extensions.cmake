
# 1.3 generate_inc_*

# These functions are useful if there is a need to generate a file
# that can be included into the application at build time. The file
# can also be compressed automatically when embedding it.
#
# See tests/application_development/gen_inc_file for an example of
# usage.
function(app_generate_inc_file
    source_file    # The source file to be converted to hex
    generated_file # The generated file
    dependencies    # The dependencies
    )
  add_custom_command(
    OUTPUT ${generated_file}
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/build/file2hex.py
    ${ARGN} # Extra arguments are passed to file2hex.py
    --file ${source_file}
    > ${generated_file} # Does pipe redirection work on Windows?
    DEPENDS ${source_file} ${dependencies}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

function(app_generate_inc_file_for_gen_target
    target          # The cmake target that depends on the generated file
    source_file     # The source file to be converted to hex
    generated_file  # The generated file
    gen_target      # The generated file target we depend on
    dependencies    # The dependencies
                    # Any additional arguments are passed on to file2hex.py
    )
    app_generate_inc_file(${source_file} ${generated_file} ${dependencies} ${ARGN})

  # Ensure 'generated_file' is generated before 'target' by creating a
  # dependency between the two targets

  add_dependencies(${target} ${gen_target})
endfunction()

function(app_generate_inc_file_for_target
    target          # The cmake target that depends on the generated file
    source_file     # The source file to be converted to hex
    generated_file  # The generated file
    dependencies    # The dependencies
                    # Any additional arguments are passed on to file2hex.py
    )
  # Ensure 'generated_file' is generated before 'target' by creating a
  # 'custom_target' for it and setting up a dependency between the two
  # targets

  # But first create a unique name for the custom target
  generate_unique_target_name_from_filename(${generated_file} generated_target_name)

  add_custom_target(${generated_target_name} DEPENDS ${generated_file})
  app_generate_inc_file_for_gen_target(${target} ${source_file} ${generated_file} ${generated_target_name} ${dependencies} ${ARGN})
endfunction()
