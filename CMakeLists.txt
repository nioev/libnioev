cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(nioev src/SubscriptionTree.cpp src/Timers.cpp)
