cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 20)
project(standalone)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
file(GLOB S_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(Standalone ${S_FILES})

add_custom_target(ReflectTestHeaders
    COMMAND python ${CMAKE_CURRENT_LIST_DIR}/../python/reflect.py --source ${CMAKE_CURRENT_SOURCE_DIR}/src
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Reflect Test Headers"
)

add_dependencies(Standalone ReflectTestHeaders)

add_subdirectory(../ vengine)
target_link_libraries(Standalone vengine)

add_custom_command ( TARGET Standalone POST_BUILD
    COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../copy_s.py "$<TARGET_RUNTIME_DLLS:vengine>" $<TARGET_FILE_DIR:Standalone>
)
add_custom_command ( TARGET Standalone POST_BUILD
    COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../copy_s.py "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/bass_main/bin/bass.dll" $<TARGET_FILE_DIR:Standalone>
)
#install(TARGETS libwave DESTINATION bin)