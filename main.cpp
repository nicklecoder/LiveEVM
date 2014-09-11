/******************************************************************************
 * PROJECT: LiveEVM
 * Author: Adam Nickle
 *
 * Senior Project 2014
 *****************************************************************************/

#include <iostream>

//#include "classes/Params.hpp"
//#include "classes/Amplifier.hpp"
#include "classes/Recorder.hpp"
#include "constants.hpp"

//Instantiate Globals
bool Running[T_COUNT] = {0};
bool Ready[T_COUNT] = {0};
std::mutex m[T_COUNT];
std::condition_variable cnd[T_COUNT];
std::thread* Threads[T_COUNT];

/**********************************************************
 * Function: main()
 *********************************************************/
int main() {
	Recorder* rec = new Recorder();
	rec->run();

	return 0;
}
