cmake_minimum_required(VERSION 2.8.8)
project(cfilb)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LLVM_LINK_COMPONENTS support)

add_clang_executable(cfilb-prep
  code-prep.cpp
  )
target_link_libraries(cfilb-prep
  clangTooling
  clangBasic
  clangASTMatchers
  )

install(TARGETS cfilb-prep RUNTIME DESTINATION bin)