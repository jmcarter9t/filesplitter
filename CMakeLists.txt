# Stops the execution of cmake if the version is less than specified. Command should be first in the top-most CMakeLists.txt file. This command IMPLICITY calls cmake_policy(VERSION X) as well.
cmake_minimum_required (VERSION 3.1)

project (filesplitter LANGUAGES CXX VERSION 0.1)

# need to set these prior to setting any targets.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-O3")
set(CMAKE_C_FLAGS "-O3")

# Adds an executable target (i.e., the runnable program file) to the project and specify which source files are used to build it.
# By default the executable target will have the name of the project.
# Target names must be GLOBALLY UNIQUE. The actual target name depends on the OS conventions (e.g., target.exe)
# By default, the executable target is written in the source directory in which the command was invoked.
# Executable target names may use generator expressions.
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/include" )
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/spdlog")

# include_directories( "${MACPORTS_DIR}/local/include" )
# link_directories( "${MACPORTS_DIR}/local/lib" "/usr/lib" "/usr/local/lib" )

# Add a new target with an empty list of sources since we add those in the src directories.
add_executable (filesplitter "")

# The include pattern is better for several reasons.
include("src/CMakeLists.txt")

