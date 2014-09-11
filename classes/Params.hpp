/*****************************************************************************
 *
 *
 ****************************************************************************/

#ifndef PARAMS_HPP
#define PARAMS_HPP

#include <atomic>

#include "../constants.hpp"

class Params
{
	private:
		Params();
		~Params();

		std::atomic<bool> mFilter; //0: Gaussian; 1: Laplacian
		std::atomic<int> mAmpLevel;
		std::atomic<int> mLowLevel;
		std::atomic<int> mHighLevel;

		static std::atomic<bool> mInstanceFlag;
		static Params* mInstance;

	public:
		static Params* getInstance();
		int getAmpLevel() {return this->mAmpLevel;};
		int getLowLevel() {return this->mLowLevel;};
		int getHighLevel() {return this->mHighLevel;};
		bool getFilter() {return this->mFilter;};

		bool setAmpLevel(const int &level);
		bool setLowLevel(const int &level);
		bool setHighLevel(const int &level);
		bool setFilter(const int &filter);
};

#endif /*PARAMS_HPP*/
