/*****************************************************************************
 *
 ****************************************************************************/
#include "Params.hpp"

//set up static vars
Params* Params::mInstance = 0;
std::atomic<bool> Params::mInstanceFlag(false);

/************************************************
 * Default constructor
 ***********************************************/
Params::Params()
{
	this->mFilter = false; //default to Gaussian
	this->mAmpLevel = 0; 
	this->mLowLevel = 0;
	this->mHighLevel = 0;
}

/************************************************
 * Default destructor
 ***********************************************/
Params::~Params()
{
	this->mInstanceFlag = false;
}

/************************************************
 * Function for retrieving the singleton instance.
 ***********************************************/
Params* Params::getInstance()
{
	if(Params::mInstanceFlag) {
		return Params::mInstance;
	} else {
		Params::mInstance = new Params();
		Params::mInstanceFlag = true;
		return Params::mInstance;
	}
}

/************************************************
 * Sets the amplification level used by the
 * amplifier worker threads.
 ***********************************************/
bool Params::setAmpLevel(const int &level)
{
    this->mAmpLevel = level;
    return true;
}

/************************************************
 * Sets the low end of the pass band for the 
 * band pass filter.
 ***********************************************/
bool Params::setLowLevel(const int &level)
{
	if(level < this->mHighLevel) {
		this->mLowLevel = level;
		return true;
	} else {
		return false;
	}
}

/************************************************
 * Sets the high end of the pass band for the
 * band pass filter.
 ***********************************************/
bool Params::setHighLevel(const int &level)
{
	if(level > this->mLowLevel) {
		this->mHighLevel = level;
		return true;
	} else {
		return false;
	}
}

/************************************************
 * Sets which filter to use when down-sampling the
 * video buffer.
 ***********************************************/
bool Params::setFilter(const int &filter)
{
	if(filter == 1 || filter == 0) {
		this->mFilter = (filter ? true : false);
		return true;
	} else {
		return false;
	}
}
