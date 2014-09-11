/******************************************************************************
 * FILE: Amplifier.hpp
 *****************************************************************************/

#ifndef AMPLIFIER_HPP
#define AMPLIFIER_HPP

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>

//OpenCV necessities
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "../constants.hpp"
#include "Params.hpp"

class Amplifier {
	public:
		Amplifier() {};
		Amplifier(int tid, cv::Mat* pSrc, cv::Mat* pDst);
		~Amplifier();

		void run();
		static cv::Mat pyrUp(cv::Mat &src);
		static void pyrUpEdges(cv::Mat &src, cv::Mat &dst);
		static cv::Mat pyrDown(cv::Mat &src);
		static void pyrDownEdges(cv::Mat &src, cv::Mat &dst);
        
        static Params* mParams;

	private:
		cv::Mat mClean[BUF_SIZE];
		cv::Mat mProcess[2 * BUF_SIZE];
		cv::Mat* mSrc;
		cv::Mat* mDst;
        cv::Mat* mOrig;

		int mCpyCursor;
		int mProcCursor;
		int mTid;

		void bandpassFilter();
		void getNewFrames();
		void getAllFrames();
		void putNewFrames();
		void initProcessBuffer();
        void invertIdx();
		void fft();
		void ifft();
		void scaleAndAmplify();


};
#endif //AMPLIFIER_HPP
