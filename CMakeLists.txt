cmake_minimum_required (VERSION 3.5)

option(AUDIO_PLAYER "Compile with Audio Player support" OFF)

if (WIN32)
    set(SDL2_DIR ${CMAKE_CURRENT_LIST_DIR}/SDL2)
endif()

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.c)
file(GLOB_RECURSE HEADER_FILES include/*.h)

if (NOT DEFINED COMPONENT_DIR)

    set(SOURCE_FILES ${SOURCE_FILES} main.cpp)
    project (vgm2wav)

    include_directories(include)
    if (AUDIO_PLAYER)
        add_definitions(-DAUDIO_PLAYER=1)
    endif()
    if (WIN32)
       include_directories(${SDL2_DIR}/include)
    endif()

    add_executable(vgm2wav ${HEADER_FILES} ${SOURCE_FILES})
    if (AUDIO_PLAYER)
        find_package(SDL2 REQUIRED)
        target_link_libraries(vgm2wav ${SDL2_LIBRARIES})
    endif()

else()

    idf_component_register(SRCS ${SOURCE_FILES}
                           INCLUDE_DIRS "include")

endif()
