enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

add_executable(symEmulator simglibc.cpp imodel.cpp)
target_link_libraries(symEmulator triton)
