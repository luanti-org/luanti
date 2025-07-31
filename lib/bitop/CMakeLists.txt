add_library(bitop STATIC bit.cpp bit.h)
target_include_directories(bitop PRIVATE ${LUA_INCLUDE_DIR})

set(LUA_BIT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(LUA_BIT_LIBRARY bitop PARENT_SCOPE)
