/******************************************************************************
 * File: Recorder.hpp
 *****************************************************************************/
#ifndef RECORDER_HPP
#define RECORDER_HPP

#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <time.h>

//OpenCV necessities
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "../constants.hpp"
#include "Params.hpp"
#include "Amplifier.hpp"

class Recorder {
	public:
		Recorder();
		~Recorder();

		void run();

		//Data Membbers
		static int mAmpLevel;
		static int mFilter;
		static int mHighLevel;
		static int mLowLevel;
        static Params* mParams;

	private:
		//Data Memers
		Amplifier* mAmps[T_COUNT];

		cv::Mat mClean[BUF_SIZE];
		cv::Mat mReduced[BUF_SIZE];
		cv::Mat mAmplified[BUF_SIZE];
		cv::VideoCapture cap;

		int mCursor;
		int mThreadIdx;
		int tid;
		bool mActiveFilter;

		//Methods
		void displayFrame();
		void getFrame();
		void initAmplifiers();
		void initBuffers();
		void runAmp();

		//Sliders
		static void onAmpSlider(int val, void* userData);
		static void onFilterSlider(int val, void* userData);
		static void onHighSlider(int val, void* userData);
		static void onLowSlider(int val, void* userData);
};
#endif //RECORDER_HPP
