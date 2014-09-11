/*****************************************************************************
 *
 ****************************************************************************/
#include "Params.hpp"

//set up static vars
Params* Params::mInstance = 0;
std::atomic<bool> Params::mInstanceFlag(false);

/************************************************
 *
 ***********************************************/
Params::Params()
{
	this->mFilter = false; //default to Gaussian
	this->mAmpLevel = 0; 
	this->mLowLevel = 0;
	this->mHighLevel = 0;
}

/************************************************
 *
 ***********************************************/
Params::~Params()
{
	this->mInstanceFlag = false;
}

/************************************************
 *
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
 *
 ***********************************************/
bool Params::setAmpLevel(const int &level)
{
    this->mAmpLevel = level;
    return true;
}

/************************************************
 *
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
 *
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
 *
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
