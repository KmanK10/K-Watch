cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Shown in Settings and to the iPhone app. Bump it when a new build goes on the watch for real.
set(PROJECT_VER "0.5")
project(k-watch)
