/******************************************************************************
 * FILE: Constants.hpp
 *****************************************************************************/
#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <thread>
#include <mutex>
#include <condition_variable>

const int BUF_SIZE = 64;
const int T_COUNT = 3;
const int PYR_LEVELS = 4;


extern bool Running[T_COUNT];
extern bool Ready[T_COUNT];                                 
extern std::mutex m[T_COUNT];
extern std::condition_variable cnd[T_COUNT];
extern std::thread* Threads[T_COUNT];

#endif //CONSTANTS_HPP
