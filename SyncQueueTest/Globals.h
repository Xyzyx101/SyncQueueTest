#pragma once
#include <chrono>

using namespace std::chrono_literals;

inline const std::chrono::high_resolution_clock::duration SYNC_WORK_DURATION{ 50us };
inline const std::chrono::high_resolution_clock::duration ASYNC_WORK_DURATION{ 50ms };
inline const std::chrono::high_resolution_clock::duration RECEIVE_DURATION{ 1s };
inline const int USERS{ 4 };
