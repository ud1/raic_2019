cmake_minimum_required(VERSION 3.14)
project(aicup2019)

set(CMAKE_CXX_STANDARD 17)

# -D_GLIBCXX_DEBUG
SET(CMAKE_CXX_FLAGS "-D_LINUX -O3 -g -Wall -Wno-unknown-pragmas -Werror=return-type -DENABLE_LOGGING -DCUSTOM_RUNNER ")

file(GLOB HEADERS "*.hpp" "model/*.hpp" )
SET_SOURCE_FILES_PROPERTIES(${HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
file(GLOB SRC "*.cpp" "model/*.cpp" )
add_executable(aicup2019 ${HEADERS} ${SRC})
TARGET_LINK_LIBRARIES(aicup2019 ${PROJECT_LIBS} pthread boost_system boost_timer boost_iostreams boost_filesystem)

