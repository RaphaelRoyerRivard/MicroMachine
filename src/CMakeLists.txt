set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# All the source files for the bot.
file(GLOB BOT_SOURCES "*.cpp" "*.h" "*.hpp")

# Enable compilation of the SC2 version of the bot.
add_definitions(-DSC2API)

include_directories(SYSTEM "${SC2Api_INCLUDE_DIRS}" "/var/lib/jenkins/workspace/MicroMachine/precompiled_libvoxelbot/include")
# link_directories("/var/lib/jenkins/workspace/MicroMachine/precompiled_libvoxelbot/lib")
# add_library(LIBVOXELBOT STATIC "/var/lib/jenkins/workspace/MicroMachine/precompiled_libvoxelbot/lib/libvoxelbot_release.lib")

# Show more warnings at compiletime.
if (MSVC)
    # FIXME: put Windows specific options here.
else ()
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
endif ()

# Create the executable.
add_executable(CommandCenter ${BOT_SOURCES})
target_link_libraries(CommandCenter ${SC2Api_LIBRARIES} "/var/lib/jenkins/workspace/MicroMachine/sc2-libvoxelbot/build/libvoxelbot_release.lib")

if (APPLE)
    target_link_libraries(CommandCenter "-framework Carbon")
endif ()

# Linux specific.
if (UNIX AND NOT APPLE)
    target_link_libraries(CommandCenter pthread dl)
endif ()
