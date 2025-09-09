cmake_minimum_required(VERSION 3.16)

# ###############################
# Generic CMake config
# ###############################
project(main)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/build)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build/lib/release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build/lib/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/build/bin/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build/lib/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build/lib/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build/bin/debug")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

# ###############################
# Custom functions
# ###############################
function(copyTemps target)
    if(SAVETEMPS STREQUAL "--save-temps")
        set(cmd1 mkdir -p ${CMAKE_BINARY_DIR}/build/bin/debug/temps/${target} || true)
        set(cmd2 mv *.bc ${CMAKE_BINARY_DIR}/build/bin/debug/temps/${target} || true)
        set(cmd3 mv ${CMAKE_BINARY_DIR}/src/${target}/CMakeFiles/${target}.dir/*.o ${CMAKE_BINARY_DIR}/build/bin/debug/temps/${target} || true)
        set(cmd4 mv *.i ${CMAKE_BINARY_DIR}/build/bin/debug/temps/${target} || true)
        set(cmd5 mv *.ii ${CMAKE_BINARY_DIR}/build/bin/debug/temps/${target} || true)
        set(cmd6 mv *.s ${CMAKE_BINARY_DIR}/build/bin/debug/temps/${target} || true)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${cmd1}>"
            COMMAND_EXPAND_LISTS
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${cmd2}>"
            COMMAND_EXPAND_LISTS
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${cmd3}>"
            COMMAND_EXPAND_LISTS
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${cmd4}>"
            COMMAND_EXPAND_LISTS
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${cmd5}>"
            COMMAND_EXPAND_LISTS
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${cmd6}>"
            COMMAND_EXPAND_LISTS
        )
        unset(cmd1)
        unset(cmd2)
        unset(cmd3)
        unset(cmd4)
        unset(cmd5)
        unset(cmd6)
    endif()
endfunction()

function(getTestBinaryName pathToBinary)
    string(FIND ${pathToBinary} "/" fileNameIndex REVERSE)
    MATH(EXPR fileNameIndex_ "${fileNameIndex}+1")
    string(SUBSTRING ${pathToBinary} ${fileNameIndex_} -1 fileName)
    set(fileName ${fileName} PARENT_SCOPE)
endfunction()

function(getTestName pathToBinary)
    string(FIND ${file} "/" fileNameIndex REVERSE)
    MATH(EXPR fileNameIndex_ "${fileNameIndex}+1")
    string(SUBSTRING ${file} ${fileNameIndex_} -1 fileName)
    string(REPLACE ".c" "" testName ${fileName})
    set(testName ${testName} PARENT_SCOPE)
endfunction()

function(getSandboxBinaryName pathToBinary)
    string(FIND ${pathToBinary} "/" fileNameIndex REVERSE)
    MATH(EXPR fileNameIndex_ "${fileNameIndex}+1")
    string(SUBSTRING ${pathToBinary} ${fileNameIndex_} -1 fileName)
    set(fileName ${fileName} PARENT_SCOPE)
endfunction()

function(getSandboxName pathToBinary)
    string(FIND ${file} "/" fileNameIndex REVERSE)
    MATH(EXPR fileNameIndex_ "${fileNameIndex}+1")
    string(SUBSTRING ${file} ${fileNameIndex_} -1 fileName)
    string(REPLACE ".c" "" sandboxName ${fileName})
    set(sandboxName ${sandboxName} PARENT_SCOPE)
endfunction()

function(setupTests)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        include_directories(/usr/local/include/)
        link_directories(/usr/local/lib/)

        include(CTest)
        enable_testing()

        file(GLOB files "test/*.c")

        foreach(file ${files})
            getTestName("${file}")
            add_executable(${testName} ${file})
            target_link_libraries(
                ${testName} PRIVATE
                /usr/local/lib/libunity.a
                SHMLibrary
                OSMPLibrary -Wl,--allow-multiple-definition # required for using mocks and real functions together within same compile unit, this will give precedence to mocked library functions contained in the tests.
            )

            set_target_properties(${testName}
                PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build/bin/debug/tests"
            )

            copyTemps(${testName})

            # required if one wants to exchange mock functions with library functions; don't use with '-Wl,--allow-multiple-definition'
            # since otherwise the predence of mocked functions is not guaranteed (config row is commented out by default within config.h.in,
            # comment it in in order to use that feature).
            set(FFF_GCC_FUNCTION_ATTRIBUTES "__attribute__((weak))")
            configure_file(test/config.h.in test/config.h)

            # set(CMAKE_C_FLAGS_DEBUG_ "${CMAKE_C_FLAGS_DEBUG} -ldflag=-Wl,--allow-multiple-definition")
            # set(CMAKE_C_FLAGS_RELEASE_ "${CMAKE_C_FLAGS_RELEASE} -ldflag=-Wl,--allow-multiple-definition")
            # target_compile_options(${testName} PRIVATE "$<$<CONFIG:DEBUG>:SHELL:${CMAKE_C_FLAGS_DEBUG_}>")
            # target_compile_options(${testName} PRIVATE "$<$<CONFIG:RELEASE>:SHELL:${CMAKE_C_FLAGS_RELEASE_}>")
            # only if tests should be run via ctest
            add_test(NAME ${testName}_suite COMMAND ${testName}_suite_tests)
        endforeach()
    endif()
endfunction()

function(setupSandbox)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB files "sandbox/*.c")

        foreach(file ${files})
            getSandboxName("${file}")
            add_executable(${sandboxName} ${file})
            target_link_libraries(
                ${sandboxName} PRIVATE
                ncurses menu

                # /usr/local/lib/libunity.a
                # SHMLibrary
                # OSMPLibrary -Wl,--allow-multiple-definition #required for using mocks and real functions together within same compile unit, this will give precedence to mocked library functions contained in the tests.
            )

            set_target_properties(${sandboxName}
                PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/build/bin/debug/sandbox"
            )

            copyTemps(${sandboxName})
        endforeach()
    endif()
endfunction()

function(setupCoverage)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB files "test/*.c")

        if(USE_GCC)
            include(CodeCoverage)
            set(coverage_dependencies "")

            set(coverage_exclude_paths
                "/usr/include/*"
                "/home/vscode/.conan/*"
                "_deps/googletest-src/googlemock/*"
                "_deps/googletest-src/googletest/*"
                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/*"
                "src/Communication/tests/*"
                "src/Library/tests/*"
                "src/protos/*"
                "src/main.cpp"
            )

            # Add target for global coverage
            SETUP_TARGET_FOR_COVERAGE_LCOV(
                NAME coverage_lcov
                BASE_DIRECTORY "${CMAKE_BINARY_DIR}"
                EXECUTABLE ctest -j ${n_cores} # Executable in PROJECT_BINARY_DIR
                DEPENDENCIES
                ${coverage_dependencies}
                EXCLUDE
                ${coverage_exclude_paths}
            )

            SETUP_TARGET_FOR_COVERAGE_GCOVR_XML(
                NAME coverage_xml
                BASE_DIRECTORY "${CMAKE_BINARY_DIR}"
                EXECUTABLE ctest -j ${n_cores} # Executable in PROJECT_BINARY_DIR
                DEPENDENCIES
                ${coverage_dependencies}
                EXCLUDE
                ${coverage_exclude_paths}
            )

            SETUP_TARGET_FOR_COVERAGE_GCOVR_HTML(
                NAME coverage_html
                BASE_DIRECTORY "${CMAKE_BINARY_DIR}"
                EXECUTABLE ctest -j ${n_cores} # Executable in PROJECT_BINARY_DIR
                DEPENDENCIES
                ${coverage_dependencies}
                EXCLUDE
                ${coverage_exclude_paths}
            )
        endif(USE_GCC)

        if(USE_LLVM)
            cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME dirName)
            set(generate_llvm_coverage generate_llvm_coverage_${dirName})
            set(generate_llvm_coverage_ generate_llvm_coverage_${dirName}_)

            add_custom_target(
                ${generate_llvm_coverage}
                DEPENDS ${generate_llvm_coverage_}
                VERBATIM
            )

            add_custom_command(

                # clean up
                OUTPUT ${generate_llvm_coverage_}
                COMMAND find . -name "*.gcda" -print0 | xargs -0 -r rm -rf
                VERBATIM
            )

            foreach(file ${files})
                getTestName("${file}")
                add_custom_command(
                    OUTPUT ${generate_llvm_coverage_} APPEND
                    COMMAND LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/build/${testName}.profraw ${CMAKE_BINARY_DIR}/build/bin/debug/tests/${testName}
                    COMMAND llvm-profdata-15 merge -sparse ${CMAKE_BINARY_DIR}/build/${testName}.profraw -o ${CMAKE_BINARY_DIR}/build/${testName}.profdata
                    COMMAND llvm-cov-15 export --ignore-filename-regex=.*tests\.c --format=lcov ${CMAKE_BINARY_DIR}/build/bin/debug/tests/${testName} -instr-profile=${CMAKE_BINARY_DIR}/build/${testName}.profdata > ./${testName}.lcov
                    COMMAND genhtml -o ${CMAKE_BINARY_DIR}/build/report/coverage/html ${CMAKE_BINARY_DIR}/build/${testName}.lcov
                    COMMAND mv ${CMAKE_BINARY_DIR}/build/${testName}.lcov ${CMAKE_BINARY_DIR}/build/report/coverage/${testName}.lcov
                    COMMAND rm -f ${CMAKE_BINARY_DIR}/build/${testName}.profdata ${CMAKE_BINARY_DIR}/build/${testName}.profraw
                    VERBATIM
                )
            endforeach()

            set_source_files_properties(${generate_llvm_coverage_} PROPERTIES SYMBOLIC "true")
        endif(USE_LLVM)
    endif()
endfunction()

function(setupTotalCoverage)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(GLOB files "build/build/bin/debug/tests/*_tests")

        if(USE_GCC)
        endif(USE_GCC)

        if(USE_LLVM)
            add_custom_target(
                generate_llvm_coverage
                DEPENDS generate_llvm_coverage_
                VERBATIM
            )

            add_custom_command(

                # clean up
                OUTPUT generate_llvm_coverage_
                COMMAND find . -name "*.gcda" -print0 | xargs -0 -r rm -rf

                # COMMAND ${CMAKE_COMMAND} -E echo "ATest"
                VERBATIM
            )

            # create the profile files
            foreach(file ${files})
                getTestBinaryName("${file}")
                add_custom_command(
                    OUTPUT generate_llvm_coverage_ APPEND
                    COMMAND LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/build/${fileName}.profraw ${CMAKE_BINARY_DIR}/build/bin/debug/tests/${fileName}
                    VERBATIM
                )
            endforeach()

            # merge the profile files
            set(objectString "")

            foreach(file ${files})
                getTestBinaryName("${file}")
                list(APPEND objectString "${CMAKE_BINARY_DIR}/build/${fileName}.profraw")
            endforeach()

            add_custom_command(
                OUTPUT generate_llvm_coverage_ APPEND
                COMMAND llvm-profdata-15 merge -sparse -o ${CMAKE_BINARY_DIR}/build/total.profdata ${objectString}
            )

            # clean up profile files
            foreach(file ${files})
                getTestBinaryName("${file}")
                add_custom_command(
                    OUTPUT generate_llvm_coverage_ APPEND
                    COMMAND rm -f ${CMAKE_BINARY_DIR}/build/${fileName}.profraw
                    VERBATIM
                )
            endforeach()

            # generate the coverage report
            set(objectString "")

            foreach(file ${files})
                getTestBinaryName("${file}")
                list(APPEND objectString "-object")
                list(APPEND objectString "${CMAKE_BINARY_DIR}/build/bin/debug/tests/${fileName}")
            endforeach()

            add_custom_command(
                OUTPUT generate_llvm_coverage_ APPEND
                COMMAND llvm-cov-15 export --ignore-filename-regex=.*tests\.c --format=lcov ${objectString} -instr-profile=${CMAKE_BINARY_DIR}/build/total.profdata > ${CMAKE_BINARY_DIR}/build/total.lcov
                VERBATIM
            )

            # generate the export & clean up
            add_custom_command(
                OUTPUT generate_llvm_coverage_ APPEND
                COMMAND genhtml -o ${CMAKE_BINARY_DIR}/build/report/coverage/html ${CMAKE_BINARY_DIR}/build/total.lcov
                COMMAND mv ${CMAKE_BINARY_DIR}/build/total.lcov ${CMAKE_BINARY_DIR}/build/report/coverage/total.lcov
                COMMAND rm -f ${CMAKE_BINARY_DIR}/build/total.profdata
                VERBATIM
            )
            set_source_files_properties(${generate_llvm_coverage_} PROPERTIES SYMBOLIC "true")
        endif(USE_LLVM)
    endif()
endfunction()

# ###############################
# Set up packages
# ###############################
find_package(PkgConfig)

if(USE_GCC)
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_C_FLAGS_DEBUG "-ggdb3 -O0 -save-temps=cwd -std=c99 -Wall --coverage")
    set(CMAKE_C_FLAGS_RELEASE "-std=c99 -Wall ")
    set(CMAKE_CXX_COMPILER g++)
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -save-temps=cwd -std=c++20 -Wall --coverage")
    set(CMAKE_CXX_FLAGS_RELEASE "-std=c++20 -Wall ")

    # set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,--whole-archive")
endif(USE_GCC)

if(USE_LLVM)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_C_FLAGS_DEBUG "${SAVETEMPS} -std=c99 -Wall -Werror -fprofile-arcs -ftest-coverage -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_C_FLAGS_RELEASE "-std=c99 -Wall -Werror")
    set(CMAKE_CXX_COMPILER clang++)
    set(CMAKE_CXX_FLAGS_DEBUG "${SAVETEMPS} -std=c99 -Wall -Werror -fprofile-arcs -ftest-coverage -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_CXX_FLAGS_RELEASE "-std=c99 -Wall -Werror ")

    # set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-lgcov")
endif(USE_LLVM)

# ###############################
# Modules, Libraries and Linking
# ###############################
set(usb_ws2812_lib "usb-ws2812-lib")
add_subdirectory(src/${usb_ws2812_lib})


set(usb_ws2812_module "usb-ws2812")
add_subdirectory(src/${usb_ws2812_module})

set(usb_ws2812_min_example "usb-ws2812-minimal-example")
add_subdirectory(src/${usb_ws2812_min_example})

# setupTotalCoverage()

# setupSandbox()
add_custom_target(
    clean_profiles
    COMMAND find . -name "*.gcda" -print0 | xargs -0 -r rm -rf
    VERBATIM
)
