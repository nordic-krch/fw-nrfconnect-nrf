include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

set(unity_products_dir "${APPLICATION_BINARY_DIR}/unity")
set(cmock_products_dir "${unity_products_dir}/mocks")
set(CMOCK_PATH "$ENV{ZEPHYR_BASE}/../cmock")
file(MAKE_DIRECTORY "${unity_products_dir}")


#cmocking function
function(cmock_generate header_path dst_path)
  set(ruby_cmd "ruby")
  set(cmock_script "${CMOCK_PATH}/lib/cmock.rb")
  set(cmock_plugins "--plugins=return_thru_ptr\;ignore_arg\;ignore\;callback\;array")
  set(cmock_prefix "--mock_prefix=mock_")
  set(cmock_dst "--mock_path=${dst_path}")
  set(cmock_header ${header_path})

  file(MAKE_DIRECTORY "${dst_path}")

  execute_process(
    COMMAND ${ruby_cmd} ${cmock_script} 
    ${cmock_prefix} ${cmock_plugins} 
    ${cmock_dst} ${cmock_header}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE op_result
    OUTPUT_VARIABLE output_result
  )
  message("${OUTPUT_VARIABLE}")
  message(STATUS ${header_path})
  message(STATUS ${cmock_products_dir})
  target_include_directories(app PRIVATE ${cmock_products_dir})
  #target_include_directories(app PRIVATE ${dst_path})
  FILE(GLOB app_sources ${dst_path}/*.c)
  target_sources(app PRIVATE ${app_sources})
endfunction()

#cmocking function
function(test_runner_generate test_file_path)
    
    set(unity_runner_cmd "/usr/bin/ruby")
    set(unity_runner_arg1 "${CMOCK_PATH}/vendor/unity/auto/generate_test_runner.rb")
    set(unity_runner_arg2 "${test_file_path}")
    get_filename_component(test_file_name "${test_file_path}" NAME)
    set(unity_runner_arg3 "${unity_products_dir}/runner/runner_${test_file_name}")

    file(MAKE_DIRECTORY "${unity_products_dir}/runner")
    execute_process(
      COMMAND ${unity_runner_cmd} ${unity_runner_arg1} ${unity_runner_arg2} ${unity_runner_arg3}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      RESULT_VARIABLE op_result
    )
    FILE(GLOB app_sources ${unity_products_dir}/runner/*.c)
    target_sources(app PRIVATE ${app_sources})
endfunction()

#function that handles --wrap linker option
function(cmock_linker_trick func_name_path)
    FILE(READ "${func_name_path}" contents)
    STRING(REGEX REPLACE "\n" ";" contents "${contents}")
    set(linker_str "-Wl")
    foreach(src ${contents})
        set(linker_str "${linker_str},--wrap=${src}")
    endforeach()
    zephyr_link_libraries(${linker_str})
endfunction()


#function that handles --wrap linker option from header file
function(cmock_linker_wrap_trick header_file_path)
    set(FUNC_LIST_SCRIPT "$ENV{ZEPHYR_BASE}/../nrf/scripts/unity/func_name_list.py")
    set(FUNC_LIST_SCRIPT_ARG_INPUT "--input")
    set(FUNC_LIST_SCRIPT_ARG_OUTPUT "--output")
    set(FUNC_LIST_SCRIPT_ARG_OUTPUT_FILE "${header_file_path}.flist")

    execute_process(
      COMMAND
      "python"
      ${FUNC_LIST_SCRIPT}
      ${FUNC_LIST_SCRIPT_ARG_INPUT}
      ${header_file_path}
      ${FUNC_LIST_SCRIPT_ARG_OUTPUT}
      ${FUNC_LIST_SCRIPT_ARG_OUTPUT_FILE}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      RESULT_VARIABLE op_result
      OUTPUT_VARIABLE output_result
    )
    
    cmock_linker_trick(${FUNC_LIST_SCRIPT_ARG_OUTPUT_FILE})
endfunction()

# Function takes original header and prepares two version
# - version with system calls removed and static inline functions
#   converted to standard function declarations
# - version with addtional __wrap_ prefix for all functions that
#   is used to generate cmock
function(cmock_headers_prepare in_header out_header wrap_header)
    set(HEADER_PREPARE_SCRIPT "$ENV{ZEPHYR_BASE}/../nrf/scripts/unity/header_prepare.py")
                                                                               
    execute_process(                                                            
      COMMAND                                                                   
      "python"                                                                  
      ${HEADER_PREPARE_SCRIPT}                                                       
      "--input" ${in_header} 
      "--output" ${out_header}
      "--wrap" ${wrap_header}                                            
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}                                   
      RESULT_VARIABLE op_result                                                 
      OUTPUT_VARIABLE output_result                                             
    )                         
endfunction()

#function for handling usage of mock
#optional second argument can contain offset that include should be placed in
#for example if file under test is include mocked header as <foo/header.h> then
# mock and replaced header should be placed in <mock_path>/foo with <mock_path>
# added as include path.
function(cmock_handle header_file)
    
    #get optional offset macro
    set (extra_macro_args ${ARGN})
    list(LENGTH extra_macro_args num_extra_args)
    if (${num_extra_args} EQUAL 1)
        list(GET extra_macro_args 0 optional_offset)
        set(dst_path "${cmock_products_dir}/${optional_offset}")
    else()
        set(dst_path "${cmock_products_dir}")
    endif()

    file(MAKE_DIRECTORY "${dst_path}/internal")

    get_filename_component(header_name "${header_file}" NAME)
    set(mod_header_path "${dst_path}/${header_name}")
    set(wrap_header "${dst_path}/internal/${header_name}")

    cmock_headers_prepare(${header_file} ${mod_header_path} ${wrap_header})
    MESSAGE("!!!!!${mod_header_path} ${wrap_header}")
    cmock_generate(${wrap_header} ${dst_path})
    
    cmock_linker_wrap_trick(${mod_header_path})
endfunction()
