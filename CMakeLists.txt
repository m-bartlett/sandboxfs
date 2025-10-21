cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "sandboxfs")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(GIT_TAG)
        # Remove 'v' prefix if present (e.g., v1.0.0 -> 1.0.0)
        string(REGEX REPLACE "^v" "" PROJECT_VERSION ${GIT_TAG})
    else()
        set(PROJECT_VERSION "0.0.0")
    endif()
else()
    set(PROJECT_VERSION "0.0.0")
endif()

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O3")

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB ALL_C_FILES "${SOURCE_DIR}/*.c")
set(SOURCE_FILES "")
foreach(C_FILE ${ALL_C_FILES})
    list(APPEND SOURCE_FILES ${C_FILE})
endforeach()

file(GLOB ALL_H_FILES "${SOURCE_DIR}/*.h")
set(HEADER_FILES "")
foreach(H_FILE ${ALL_H_FILES})
    list(APPEND HEADER_FILES ${H_FILE})
endforeach()

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR})

# Link against libcap for capability management
target_link_libraries(${PROJECT_NAME} cap)

option(NO_BIND_TMP "Don't bind mount root /tmp into the sandbox's /tmp" OFF)

if(NO_BIND_TMP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NO_BIND_TMP)
endif()

include(GNUInstallDirs)

install(PROGRAMS $<TARGET_FILE:${PROJECT_NAME}>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME ${PROJECT_NAME}
)

install(CODE "
    execute_process(
        COMMAND setcap cap_sys_admin,cap_setuid+ep \${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}
    )
")

add_custom_target(install-local
    COMMAND setcap cap_sys_admin,cap_setuid+ep $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    COMMENT "Installing locally with CAP_SYS_ADMIN and CAP_SETUID capabilities"
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E echo "Removing ${PROJECT_NAME} from ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}..."
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E echo "Uninstallation complete."
    COMMENT "Uninstalling ${PROJECT_NAME}"
)