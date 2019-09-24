set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-rdynamic")

# All the source files for the bot.
file(GLOB BOT_SOURCES "*.cpp" "*.h" "*.hpp" "libvoxelbot/*/*.cpp" "libvoxelbot/*/*.h")

# Enable compilation of the SC2 version of the bot.
add_definitions(-DSC2API)

# Find libvoxelbot lib
# find_path(LIBVOXELBOT_PATH
#     NAMES
#         "build/liblibvoxelbot.a"
#     PATHS
#         "/opt/local/include/sc2-libvoxelbot"
#         "/usr/local/include/sc2-libvoxelbot"
#         "/usr/include/sc2-libvoxelbot"
#         "/var/lib/jenkins/workspace/MicroMachine/sc2-libvoxelbot"
# )

# include_directories(SYSTEM "${SC2Api_INCLUDE_DIRS}" "${LIBVOXELBOT_PATH}")
include_directories(SYSTEM "${SC2Api_INCLUDE_DIRS}" "~/Projects/MicroMachine")

# Show more warnings at compiletime.
if (MSVC)
    # FIXME: put Windows specific options here.
else ()
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
endif ()

# Create the executable.
add_executable(MicroMachine ${BOT_SOURCES})
# target_link_libraries(MicroMachine ${SC2Api_LIBRARIES} "${LIBVOXELBOT_PATH}/build/liblibvoxelbot.a")
target_link_libraries(MicroMachine ${SC2Api_LIBRARIES})

if (APPLE)
    target_link_libraries(MicroMachine "-framework Carbon")
endif ()

# Linux specific.
if (UNIX AND NOT APPLE)
    target_link_libraries(MicroMachine pthread dl)
endif ()
