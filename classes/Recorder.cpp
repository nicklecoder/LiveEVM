/******************************************************************************
 * File: Recorder.cpp
 * 
 * TODO: implement parameter passing to threads
 *****************************************************************************/
#include "Recorder.hpp"

//static members
Params* Recorder::mParams = Params::getInstance();
int Recorder::mAmpLevel = 0;
int Recorder::mFilter = 0;
int Recorder::mHighLevel = 0;
int Recorder::mLowLevel = 0;

/**********************************************************
 *
 *********************************************************/
Recorder::Recorder() {
	// -1 grabs any available camera device.
	this->cap = cv::VideoCapture(1);
	if (!this->cap.isOpened()) {
		std::cerr << "Error opening video stream. Aborting.\n";
		exit(-1);
	}
	//this->mClean = cv::Mat[BUF_SIZE];
	//this->mAmplified = new cv::Mat[BUF_SIZE];
	//this->mReduced = new cv::Mat[BUF_SIZE];
	/*for (int i = 0; i < T_COUNT; i++) {
		this->mAmps[i] = new Amplifier(i, this->mReduced, this->mAmplified);
	}*/
	this->mCursor = 0;
	this->mThreadIdx = 0;
	this->mActiveFilter = false; //default to Gaussian

	//Set up the windows
	cv::namedWindow("Clean", CV_WINDOW_AUTOSIZE);
	cv::namedWindow("EVM", CV_WINDOW_AUTOSIZE);

	cv::createTrackbar("Filter", "EVM", &(this->mFilter), 1, this->onFilterSlider);
	cv::createTrackbar("AmpLevel", "EVM", &(this->mAmpLevel), 50, this->onAmpSlider);
	cv::createTrackbar("High", "EVM", &(this->mHighLevel), BUF_SIZE, this->onHighSlider);
	cv::createTrackbar("Low", "EVM", &(this->mLowLevel), BUF_SIZE, this->onLowSlider);
    
    this->initBuffers();
    this->initAmplifiers();
}

/**********************************************************
 *
 *********************************************************/
Recorder::~Recorder() {
	//delete [] this->mClean;
	//delete [] this->mAmplified;
	//delete [] this->mReduced;
}

/**********************************************************
 *
 *********************************************************/
void Recorder::run() {
	//Buffers were inited in the constructor
	unsigned int frameCount = 0;
	while (1) {
		this->displayFrame();
		if (frameCount >= BUF_SIZE / 4) {
			frameCount = 0;
			this->runAmp();
		}
		this->getFrame();
		this->mCursor = (this->mCursor + 1) % BUF_SIZE;
		frameCount++;

		//check for escape
		if ((cvWaitKey(1) & 255) == 27) {
			break;
		}
	}

	//spin down the threads
	for (int i = 0; i < T_COUNT; i++) {
		//we don't care much about synchronization here
		Running[i] = false;
	}
	//Allow threads to exit on their own; explicit joins cause problems here
}

/**********************************************************
 * Function: displayFrame()
 *
 * Note: call this before recording new frames over the
 * cursor mark.
 *********************************************************/
void Recorder::displayFrame() {
    //if(!this->mClean[this->mCursor].empty() && !this->mAmplified[this->mCursor].empty()) {
        cv::imshow("Clean", this->mClean[this->mCursor]);
        cv::imshow("EVM", this->mAmplified[this->mCursor]);
    //}
}

/**********************************************************
 * Function: getFrame()
 *
 * Note: call this after the frames at mCursor have been
 * displayed.
 *********************************************************/
void Recorder::getFrame() {
	cv::Mat frame;
	this->cap >> frame;
	if (!frame.empty()) {
		frame.copyTo(this->mClean[this->mCursor]);
        frame.copyTo(this->mAmplified[this->mCursor]);
        usleep(60000);
	} else {
		// We failed to get a frame for some reason; abort
		std::cerr << "Failed to get a frame. Aborting.\n";
		exit(-1);
	}
}

/**********************************************************
 * Function: initAmplifiers
 *
 * Note: we create T_COUNT amplifiers, which should be one
 * less than the buffer partition count.
 *********************************************************/
void Recorder::initAmplifiers() {
	for(int i = 0; i < T_COUNT; i++) {
		//construct the new amplifiers here
		this->mAmps[i] = new Amplifier(i,  this->mClean, this->mAmplified);
	}
}

/**********************************************************
 * Function: initBuffers()
 *
 * Fills the clean and amplified frame buffers with clean
 * frames and resets mCursor
 *********************************************************/
void Recorder::initBuffers() {
	//fill the frame buffers
	for (int i = 0; i < BUF_SIZE; i++) {
		this->getFrame();
        this->mCursor = (this->mCursor + 1) % BUF_SIZE;
	}
    this->mCursor = 0;
	//Note: mCursor will be set later according to the thread index
}

/**********************************************************
 *
 *********************************************************/
cv::Mat laplace(cv::Mat src) {
}

/**********************************************************
 *
 *********************************************************/
cv::Mat Recorder::pyrUp(cv::Mat &src) {
	int tRows = src.rows * 2;
	int cols = src.cols * 2;
	int channels = src.channels();
	cv::Mat dst(cv::Size(cols, tRows), src.type());
	pyrUpEdges(src, dst);
	int evenKernel[3] = {1, 6, 1};

	//loop through main area of the frame
	double* rows[3];
	double gVal, bVal, rVal;
	for(int r = 1; r < tRows - 2; r += 2) {
		rows[0] = src.ptr<double>(r/2);
		rows[1] = src.ptr<double>(r/2 + 1);
		if(r < tRows - 3) {
			rows[2] = src.ptr<double>(r/2 + 2);
		}
		double* t1 = dst.ptr<double>(r);
		double* t2 = dst.ptr<double>(r+1);
		//handle odd rows
		for(int c = 1; c < cols - 2; c++) {
			//handle odd columns
			gVal = bVal = rVal = 0;
			for(int i = 0; i < 2; i++) {
				gVal += (rows[i][channels * (c/2)] + rows[i][channels * (c/2 + 1)]);
				bVal += (rows[i][channels * (c/2) + 1] + rows[i][channels * (c/2 + 1) + 1]);
				rVal += (rows[i][channels * (c/2) + 2] + rows[i][channels * (c/2 + 1) + 2]);
			}
			t1[channels * c] = gVal / 4;
			t1[channels * c + 1] = bVal / 4;
			t1[channels * c + 2] = rVal / 4;
			//handle even columns
			gVal = bVal = rVal = 0;
			c++;
			if(c >= cols - 2) {
				break;
			}
			for(int i = 0; i < 2; i++) {
				gVal += ((rows[i][channels * (c/2 - 1)] + rows[i][channels * (c/2 + 1)]) + 
					      rows[i][channels * (c/2)] * 6);
				bVal += ((rows[i][channels * (c/2 - 1) + 1] + rows[i][channels * (c/2 + 1) + 1]) +
					      rows[i][channels * (c/2) + 1] * 6);
				rVal += ((rows[i][channels * (c/2 - 1) + 2] + rows[i][channels * (c/2 + 1) + 2]) + 
						   rows[i][channels * (c/2) + 2] * 6);
			}
			t1[channels * c] = gVal / 16;
			t1[channels * c + 1] = bVal / 16;
			t1[channels * c + 2] = rVal / 16;
		}


		if(r > tRows - 3) break;
		//handle even rows
		for(int c = 1; c < cols - 2; c++) {
			//handle odd columns
			gVal = bVal = rVal = 0;
			for(int i = 0; i < 3; i++) {
				gVal += (rows[i][channels * (c/2)] + 
							rows[i][channels * (c/2 + 1)])*evenKernel[i];
				bVal += (rows[i][channels * (c/2) + 1] + 
							rows[i][channels * (c/2 + 1) + 1])*evenKernel[i];
				rVal += (rows[i][channels * (c/2) + 2] + 
							rows[i][channels * (c/2 + 1) + 2])*evenKernel[i];
			}
			t2[channels * c] = gVal / 16;
			t2[channels * c + 1] = bVal / 16;
			t2[channels * c + 2] = rVal / 16;

			//handle even columns
			gVal = bVal = rVal = 0;
			c++;
			if(c >= cols - 2) {
				break;
			}
			for(int i = 0; i < 3; i++) {
				gVal += (rows[i][channels * (c/2 - 1)] + 
							rows[i][channels * (c/2 + 1)] +
					      rows[i][channels * (c/2)] * 6) * evenKernel[i];
				bVal += (rows[i][channels * (c/2 - 1) + 1] + 
							rows[i][channels * (c/2 + 1) + 1] +
					      rows[i][channels * (c/2) + 1] * 6) * evenKernel[i];
				rVal += (rows[i][channels * (c/2 - 1) + 2] + 
							rows[i][channels * (c/2 + 1) + 2] +
					      rows[i][channels * (c/2) + 2] * 6) * evenKernel[i];
			}
			t2[channels * c] = gVal / 64;
			t2[channels * c + 1] = bVal / 64;
			t2[channels * c + 2] = rVal / 64;
		}
	}
	return dst;
}

/**********************************************************
 *
 *********************************************************/
void Recorder::pyrUpEdges(cv::Mat &src, cv::Mat &dst) {
	int tRows = dst.rows;
	int cols = dst.cols;
	int channels = dst.channels();
	double gVal, bVal, rVal;

	//1: handle top-left corner
	// a) handle (0,0)
	gVal = bVal = rVal = 0;
	double* rows[3];
	rows[0] = src.ptr<double>(0);
	rows[1] = src.ptr<double>(1);
	rows[2] = NULL;
	double* t1 = dst.ptr<double>(0);
	double* t2 = dst.ptr<double>(1);


	//top-left corner
	//(0,0)
	t1[0] = (rows[0][0] * 36 + 
			  (rows[0][channels] + 
				rows[1][0]) * 6 + 
			   rows[1][channels]) / 49;
	t1[1] = (rows[0][1] * 36 + 
			  (rows[0][channels + 1] + 
				rows[1][1]) * 6 + 
			   rows[1][channels + 1]) / 49;
	t1[2] = (rows[0][2] * 36 + 
			  (rows[0][channels + 2] + 
				rows[1][2]) * 6 + 
			   rows[1][channels + 2]) / 49;

	//(1,0)
	t2[0] = ((rows[0][0] + 
				 rows[1][0]) * 24 + 
			   (rows[0][channels] + 
				 rows[1][channels]) * 4) / 56;
	t2[1] = ((rows[0][1] + 
				 rows[1][1]) * 24 + 
			   (rows[0][channels + 1] + 
				 rows[1][channels + 1]) * 4) / 56;
	t2[2] = ((rows[0][2] + 
				 rows[1][2]) * 24 + 
			   (rows[0][channels + 2] + 
				 rows[1][channels + 2]) * 4) / 56;

	//(0,1)
	t1[channels] = ((rows[0][0] + 
				        rows[0][channels]) * 24 + 
			          (rows[1][0] + 
						  rows[1][channels]) * 4) / 56;
	t1[channels + 1] = ((rows[0][1] + 
				            rows[0][channels + 1]) * 24 + 
			              (rows[1][1] + 
								rows[1][channels + 1]) * 4) / 56;
	t1[channels + 2] = ((rows[0][2] + 
				            rows[0][channels + 2]) * 24 + 
			              (rows[1][2] + 
								rows[1][channels + 2]) * 4) / 56;
	//END TOP LEFT

	//2: handle top-right corner
	//(0,cols)
	int cl = cols - 1;
	t1[channels * cl] = (rows[0][(channels * (cl/2))] * 6 + rows[1][(channels * (cl/2))]) / 7;
	t1[channels * cl + 1] = (rows[0][(channels * (cl/2)) + 1] * 6 + rows[1][(channels * (cl/2)) + 1]) / 7;
	t1[channels * cl + 2] = (rows[0][(channels * (cl/2)) + 2] * 6 + rows[1][(channels * (cl/2)) + 2]) / 7;

	//(1,cols)
	t2[channels * cl] = (rows[0][(channels * (cl/2))] + rows[1][(channels * (cl/2))]) / 2;
	t2[channels * cl + 1] = (rows[0][(channels * (cl/2)) + 1] + rows[1][(channels * (cl/2)) + 1]) / 2;
	t2[channels * cl + 2] = (rows[0][(channels * (cl/2)) + 2] + rows[1][(channels * (cl/2)) + 2]) / 2;

	//(0,cols-1)
	t1[channels * (cl - 1)] = (rows[0][channels * cl/2] * 36 + 
			                    (rows[0][channels * (cl/2 - 1)] +
										rows[1][channels * cl/2]) * 6 +
									   rows[1][channels * (cl/2 - 1)]) / 49;
	t1[channels * (cl - 1) + 1] = (rows[0][channels * cl/2 + 1] * 36 + 
			                        (rows[0][channels * (cl/2 - 1) + 1] +
										    rows[1][channels * cl/2 + 1]) * 6 +
									       rows[1][channels * (cl/2 - 1) + 1]) / 49;
	t1[channels * (cl - 1) + 2] = (rows[0][channels * cl/2 + 2] * 36 + 
			                        (rows[0][channels * (cl/2 - 1) + 2] +
										    rows[1][channels * cl/2 + 2]) * 6 +
									       rows[1][channels * (cl/2 - 1) + 2]) / 49;
	//END top-right corner

	//top row
	for(int c = 2; c < cols - 2; c++) {
		//handle even
		t1[channels * c] = ((rows[0][channels * ((c/2) - 1)] + rows[0][channels * (c/2 + 1)]) * 6 + 
				               rows[0][channels * (c/2)] * 36 +
			                 (rows[1][channels * ((c/2) - 1)] + rows[0][channels * (c/2 + 1)]) + rows[1][channels * (c/2)] * 6) / 56;
		t1[channels * c + 1] = ((rows[0][channels * (c/2 - 1) + 1] + rows[0][channels * (c/2 + 1) + 1]) * 6 + 
										 rows[0][channels * (c/2) + 1] * 36 +
			                     (rows[1][channels * (c/2 - 1) + 1] + rows[0][channels * (c/2 + 1) + 1]) + rows[1][channels * (c/2) + 1] * 6) / 56;
		t1[channels * c + 2] = ((rows[0][channels * (c/2 - 1) + 2] + rows[0][channels * (c/2 + 1) + 2]) * 6 + 
										 rows[0][channels * (c/2) + 2] * 36 +
			                     (rows[1][channels * (c/2 - 1) + 2] + rows[0][channels * (c/2 + 1) + 2]) + rows[1][channels * (c/2) + 2] * 6) / 56;

		//handle odd
		c += 1;
		t1[channels * c] = ((rows[0][channels * (c/2)] + 
									rows[0][channels * (c/2 + 1)]) * 24 + 
								  (rows[1][channels * c/2] + 
									rows[1][channels * (c/2 + 1)]) * 4) / 56;
		t1[channels * c + 1] = ((rows[0][channels * (c/2) + 1] + 
										 rows[0][channels * (c/2 + 1) + 1]) * 24 + 
										(rows[1][channels * c/2 + 1] + 
										 rows[1][channels * (c/2 + 1) + 1]) * 4) / 56;
		t1[channels * c + 2] = ((rows[0][channels * (c/2) + 2] + 
										 rows[0][channels * (c/2 + 1) + 2]) * 24 + 
										(rows[1][channels * c/2 + 2] + 
										 rows[1][channels * (c/2 + 1) + 2]) * 4) / 56;
	} //END top row

	//left and right sides
	
	for(int r = 2; r < tRows - 2; r += 2) {
		//get rows
		rows[0] = src.ptr<double>(r/2 - 1);
		rows[1] = src.ptr<double>(r/2);
		rows[2] = src.ptr<double>(r/2 + 1);
		t1 = dst.ptr<double>(r);
		t2 = dst.ptr<double>(r + 1);

		//even left
		t1[0] = ((rows[0][0] + 
					 rows[2][0] + 
					 rows[1][channels]) * 6 +
				   rows[1][0] * 36 +
					rows[0][channels] + 
					rows[2][channels]) / 56;
		t1[1] = ((rows[0][1] + 
					 rows[2][1] + 
					 rows[1][channels + 1]) * 6 +
				   rows[1][1] * 36 +
					rows[0][channels + 1] + 
					rows[2][channels + 1]) / 56;
		t1[2] = ((rows[0][2] + 
					 rows[2][2] + 
					 rows[1][channels + 2]) * 6 +
				   rows[1][2] * 36 +
					rows[0][channels + 2] + 
					rows[2][channels + 2]) / 56;

		//even right
		t1[channels * (cols - 1)] = ((rows[0][channels * ((cols/2) - 1)] + rows[2][channels * ((cols/2) - 1)]) + rows[1][channels * (cols/2 - 2)] * 6) / 8;
		t1[channels * (cols - 1) + 1] = ((rows[0][channels * (cols/2 - 1) + 1] + rows[2][channels * (cols/2 - 1) + 1]) + rows[1][channels * (cols/2 - 2) + 1] * 6) / 8;
		t1[channels * (cols - 1) + 2] = ((rows[0][channels * (cols/2 - 1) + 2] + rows[2][channels * (cols/2 - 1) + 2]) + rows[1][channels * (cols/2 - 2) + 2] * 6) / 8;

		t1[channels * (cols - 2)] = ((rows[0][channels * ((cols/2) - 1)] + rows[1][channels * ((cols/2) - 2)] + rows[2][channels * ((cols/2) - 1)]) * 6 +
				                       (rows[0][channels * ((cols/2) - 2)] + rows[2][channels * ((cols/2) - 2)]) +
											  (rows[1][channels * ((cols/2) - 1)] * 36)) / 56;
		t1[channels * (cols - 2) + 1] = ((rows[0][channels * ((cols/2) - 1) + 1] + rows[1][channels * ((cols/2) - 2) + 1] + rows[2][channels * ((cols/2) - 1) + 1]) * 6 +
				                           (rows[0][channels * ((cols/2) - 2) + 1] + rows[2][channels * ((cols/2) - 2) + 1]) +
											      (rows[1][channels * ((cols/2) - 1) + 1] * 36)) / 56;
		t1[channels * (cols - 2) + 2] = ((rows[0][channels * ((cols/2) - 1) + 2] + rows[1][channels * ((cols/2) - 2) + 2] + rows[2][channels * ((cols/2) - 1) + 2]) * 6 +
				                           (rows[0][channels * ((cols/2) - 2) + 2] + rows[2][channels * ((cols/2) - 2) + 2]) +
											      (rows[1][channels * ((cols/2) - 1) + 2] * 36)) / 56;



		//odd left
		t2[0] = ((rows[1][0] + rows[2][0]) * 24 +
			          (rows[1][channels] + rows[2][channels]) * 4) / 56;
		t2[1] = ((rows[1][1] + rows[2][1]) * 24 +
			          (rows[1][channels + 1] + rows[2][channels + 1]) * 4) / 56;
		t2[2] = ((rows[1][2] + rows[2][2]) * 24 +
			          (rows[1][channels + 2] + rows[2][channels + 2]) * 4) / 56;
		//odd right
		t2[channels * (cols - 1)] = (rows[1][channels * (cols/2 - 1)] + rows[2][channels * (cols/2 - 1)]) / 2;
		t2[channels * (cols - 1) + 1] = (rows[1][channels * (cols/2 - 1) + 1] + rows[2][channels * (cols/2 - 1) + 1]) / 2;
		t2[channels * (cols - 1) + 2] = (rows[1][channels * (cols/2 - 1) + 2] + rows[2][channels * (cols/2 - 1) + 2]) / 2;

		t2[channels * (cols - 2)] = ((rows[1][channels * (cols/2 - 1)] + rows[2][channels * (cols/2 - 1)]) * 24 +
			                          (rows[1][channels * (cols/2 - 2)] + rows[2][channels * (cols/2 - 2)]) * 4) / 56;
		t2[channels * (cols - 2) + 1] = ((rows[1][channels * (cols/2 - 1) + 1] + rows[2][channels * (cols/2 - 1) + 1]) * 24 +
			                              (rows[1][channels * (cols/2 - 2) + 1] + rows[2][channels * (cols/2 - 2) + 1]) * 4) / 56;
		t2[channels * (cols - 2) + 2] = ((rows[1][channels * (cols/2 - 1) + 2] + rows[2][channels * (cols/2 - 1) + 2]) * 24 +
			                              (rows[1][channels * (cols/2 - 2) + 2] + rows[2][channels * (cols/2 - 2) + 2]) * 4) / 56;


	}
	

	//bottom corners
	/*******************************************************************************/
	t1 = dst.ptr<double>(tRows - 1);
	t2 = dst.ptr<double>(tRows - 2);
	rows[0] = src.ptr<double>(tRows/2 - 1);
	rows[1] = src.ptr<double>(tRows/2 - 2);

	// bottom row
	for(int c = 2; c < cols - 2; c++) {
		//even
		t1[channels * c] = (rows[0][channels * (c/2)] * 6 +
			                 rows[0][channels * (c/2 - 1)] +
								  rows[0][channels * (c/2 + 1)]) / 8;
		t1[channels * c + 1] = (rows[0][channels * (c/2) + 1] * 6 +
				                  rows[0][channels * (c/2 - 1) + 1] +
										rows[0][channels * (c/2 + 1) + 1]) / 8;
		t1[channels * c + 2] = (rows[0][channels * (c/2) + 2] * 6 +
				                  rows[0][channels * (c/2 - 1) + 2] +
										rows[0][channels * (c/2 + 1) + 2]) / 8;

		//odd
		c++;
		t1[channels * c] = (rows[0][channels * (c/2)] +
				              rows[0][channels * (c/2 + 1)]) / 2;
		t1[channels * c + 1] = (rows[0][channels * (c/2) + 1] +
				                  rows[0][channels * (c/2 + 1) + 1]) / 2;
	   t1[channels * c + 2]	= (rows[0][channels * (c/2) + 2] +
				                  rows[0][channels * (c/2 + 1) + 2]) / 2;
	}

	//bottom-left corner
	//(0,0)
	t1[0] = (rows[0][0] * 36 + 
			  (rows[0][channels] + 
				rows[1][0]) * 6 + 
			   rows[1][channels]) / 49;
	t1[1] = (rows[0][1] * 36 + 
			  (rows[0][channels + 1] + 
				rows[1][1]) * 6 + 
			   rows[1][channels + 1]) / 49;
	t1[2] = (rows[0][2] * 36 + 
			  (rows[0][channels + 2] + 
				rows[1][2]) * 6 + 
			   rows[1][channels + 2]) / 49;

	//(1,0)
	t2[0] = ((rows[0][0] + 
				 rows[1][0]) * 24 + 
			   (rows[0][channels] + 
				 rows[1][channels]) * 4) / 56;
	t2[1] = ((rows[0][1] + 
				 rows[1][1]) * 24 + 
			   (rows[0][channels + 1] + 
				 rows[1][channels + 1]) * 4) / 56;
	t2[2] = ((rows[0][2] + 
				 rows[1][2]) * 24 + 
			   (rows[0][channels + 2] + 
				 rows[1][channels + 2]) * 4) / 56;

	//(0,1)
	t1[channels] = ((rows[0][0] + 
				        rows[0][channels]) * 24 + 
			          (rows[1][0] + 
						  rows[1][channels]) * 4) / 56;
	t1[channels + 1] = ((rows[0][1] + 
				            rows[0][channels + 1]) * 24 + 
			              (rows[1][1] + 
								rows[1][channels + 1]) * 4) / 56;
	t1[channels + 2] = ((rows[0][2] + 
				            rows[0][channels + 2]) * 24 + 
			              (rows[1][2] + 
								rows[1][channels + 2]) * 4) / 56;
	//END BOTTOM LEFT

	//2: handle bottom-right corner
	//(rows,cols)
	t1[(channels * cl)] = rows[0][(channels * (cl/2))];
	t1[(channels * cl) + 1] = rows[0][(channels * (cl/2)) + 1];
	t1[(channels * cl) + 2] = rows[0][(channels * (cl/2)) + 2];

	//(rows,cols-1)
	t1[channels * (cl - 1)] = (rows[0][(channels * (cl/2))] * 6 + rows[0][(channels * ((cl/2) - 1))]) / 7;
	t1[(channels * (cl - 1)) + 1] = (rows[0][(channels * (cl/2)) + 1] * 6 + rows[0][(channels * ((cl/2) - 1)) + 1]) / 7;
	t1[(channels * (cl - 1)) + 2] = (rows[0][(channels * (cl/2)) + 2] * 6 + rows[0][(channels * ((cl/2) - 1)) + 2]) / 7;

	//(1,cols)
	t2[(channels * cl)] = (rows[0][(channels * (cl/2))] * 6 + rows[1][(channels * (cl/2))]) / 7;
	t2[(channels * cl) + 1] = (rows[0][(channels * (cl/2)) + 1] * 6 + rows[1][(channels * (cl/2)) + 1]) / 7;
	t2[(channels * cl) + 2] = (rows[0][(channels * (cl/2)) + 2] * 6 + rows[1][(channels * (cl/2)) + 2]) / 7;

	t2[channels * (cl - 1)] = (rows[0][channels * (cl/2)] * 36 + 
                              (rows[0][channels * ((cl/2) - 1)] + rows[1][channels * (cl/2)]) * 6 +
                               rows[1][channels * ((cl/2) - 1)]) / 49;
	t2[channels * (cl - 1) + 1] = (rows[0][channels * (cl/2) + 1] * 36 + 
                                  (rows[0][channels * ((cl/2) - 1) + 1] + rows[1][channels * (cl/2)]) * 6 +
                                   rows[1][channels * ((cl/2) - 1) + 1]) / 49;
	t2[channels * (cl - 1) + 2] = (rows[0][channels * (cl/2) + 2] * 36 + 
                                  (rows[0][channels * ((cl/2) - 1) + 2] + rows[1][channels * (cl/2)]) * 6 +
                                   rows[1][channels * ((cl/2) - 1) + 2]) / 49;
}

/**********************************************************
 *
 *********************************************************/
cv::Mat Recorder::pyrDown(cv::Mat &src) {
	int rows = src.rows;
	int cols = src.cols;
	int channels = src.channels();
	cv::Mat dst(cv::Size(cols/2, rows/2), src.type());
	double kernel[5] = {1.0, 4.0, 6.0, 4.0, 1.0};

	pyrDownEdges(src, dst);

	double* nRows[5];
	double gVal, bVal, rVal;
	double* target;
	for(int r = 2; r < rows - 2; r += 2) {
		nRows[0] = src.ptr<double>(r-2);
		nRows[1] = src.ptr<double>(r-1);
		nRows[2] = src.ptr<double>(r);
		nRows[3] = src.ptr<double>(r+1);
		nRows[4] = src.ptr<double>(r+2);
		target = dst.ptr<double>(r/2);
		for(int c = 2; c < cols - 2; c += 2) {
			gVal = bVal = rVal = 0;
			for(int i = 0; i < 5; i++) {
				gVal += (nRows[i][channels * (c - 2)] + nRows[i][channels * (c + 2)] +
					     (nRows[i][channels * (c - 1)] + nRows[i][channels * (c + 1)]) * 4 +
						  nRows[i][channels * c] * 6) * kernel[i];
				bVal += (nRows[i][channels * (c - 2) + 1] + nRows[i][channels * (c + 2) + 1] +
					     (nRows[i][channels * (c - 1) + 1] + nRows[i][channels * (c + 1) + 1]) * 4 +
						  nRows[i][channels * c + 1] * 6) * kernel[i];
				rVal += (nRows[i][channels * (c - 2) + 2] + nRows[i][channels * (c + 2) + 2] +
					     (nRows[i][channels * (c - 1) + 2] + nRows[i][channels * (c + 1) + 2]) * 4 +
						  nRows[i][channels * c + 2] * 6) * kernel[i];
			}
			target[channels * (c/2)] = gVal / 256;
			target[channels * (c/2) + 1] = bVal / 256;
			target[channels * (c/2) + 2] = rVal / 256;
		}
	}
	return dst;
}

/**********************************************************
 *
 *********************************************************/
void Recorder::pyrDownEdges(cv::Mat &src, cv::Mat &dst) {
	int rows = src.rows;
	int cols = src.cols;
	int channels = src.channels();
	int rOffset = !(rows % 2 == 0);
	int cOffset = !(cols % 2 == 0);
	int trDiv = (cOffset) ? 165 : 121;
	int blDiv = (rOffset) ? 165 : 121;

	double kernel[5] = {1.0, 4.0, 6.0, 4.0, 1.0};

	//get offsets
	int cDiv, rDiv, brDiv;
	if(rOffset == 0) {
		// case 1: no row offset
		rDiv = 176;
		if(cOffset == 0) {
			//case 1-1: no row or column offset; 3x3 (176 c and r offset, 121 corner)
			cDiv = 176;
			brDiv = 121;
		} else {
			//case 1-2: only column offset: 3x4 (240 c offset, 165 cr offset)
			cDiv = 240;
			brDiv = 165;
		}
	} else {
		//case 2: row offset
		rDiv = 240;
		if(cOffset == 0) {
			//case 2-1: only row offset; 4x3 (176 col, 165 corner)
			cDiv = 176;
			brDiv = 165;
		} else {
			//case 2-2: row and col offset; 4x4 (240 col offset, 225 corner)
			cDiv = 240;
			brDiv = 225;
		}
	}

	//get row pointers for top and bottom
	double* tRows[3];
	tRows[0] = src.ptr<double>(0);
	tRows[1] = src.ptr<double>(1);
	tRows[2] = src.ptr<double>(2);
	double* top = dst.ptr<double>(0);

	double* bRows[4];
	bRows[0] = src.ptr<double>(rows - 4);
	bRows[1] = src.ptr<double>(rows - 3);
	bRows[2] = src.ptr<double>(rows - 2);
	bRows[3] = src.ptr<double>(rows - 1);
	double* bottom = dst.ptr<double>(rows/2 - 1);

	//top-left corner
	double gVal, bVal, rVal;
	gVal = bVal = rVal = 0;
	for(int r = 0; r < 3; r++) {
		gVal += (tRows[r][0] * 6 +
			     tRows[r][3] * 4 +
				  tRows[r][6]) * kernel[r + 2];
		bVal += (tRows[r][1] * 6 +
			     tRows[r][4] * 4 +
				  tRows[r][7]) * kernel[r + 2];
		rVal += (tRows[r][2] * 6 +
			     tRows[r][5] * 4 +
				  tRows[r][8]) * kernel[r + 2];
	}
	//assign
	top[0] = gVal / 121;
	top[1] = bVal / 121;
	top[2] = rVal / 121;
	//END top-left

	//top-right corner
	gVal = bVal = rVal = 0;
	if(cOffset) {
		//3 rows, 4 cols
		for(int r = 0; r < 3; r++) {
			gVal += ((tRows[r][channels * (cols - 4)] +
				     (tRows[r][channels * (cols - 3)] + tRows[r][channels * (cols - 1)]) * 4 +
					  tRows[r][channels * (cols - 2)] * 6) * kernel[r + 2]);
			bVal += ((tRows[r][channels * (cols - 4) + 1] +
				     (tRows[r][channels * (cols - 3) + 1] + tRows[r][channels * (cols - 1) + 1]) * 4 +
					  tRows[r][channels * (cols - 2) + 1] * 6) * kernel[r + 2]);
			rVal += ((tRows[r][channels * (cols - 4) + 2] +
				     (tRows[r][channels * (cols - 3) + 2] + tRows[r][channels * (cols - 1) + 2]) * 4 +
					  tRows[r][channels * (cols - 2) + 2] * 6) * kernel[r + 2]);
		}
	} else {
		//3 rows, 3 cols
		for(int r = 0; r < 3; r++) {
			gVal += ((tRows[r][channels * (cols - 3)] +
				     tRows[r][channels * (cols - 2)] * 4 +
					  tRows[r][channels * (cols - 1)] * 6) * kernel[r + 2]);
			bVal += ((tRows[r][channels * (cols - 3) + 1] +
				     (tRows[r][channels * (cols - 2) + 1]) * 4 +
					  tRows[r][channels * (cols - 1) + 1] * 6) * kernel[r + 2]);
			rVal += ((tRows[r][channels * (cols - 3) + 2] +
				     (tRows[r][channels * (cols - 2) + 2]) * 4 +
					  tRows[r][channels * (cols - 1) + 2] * 6) * kernel[r + 2]);
		}
	}
	top[channels * (cols/2 - 1)] = gVal / trDiv;
	top[channels * (cols/2 - 1) + 1] = bVal / trDiv;
	top[channels * (cols/2 - 1) + 2] = rVal / trDiv;
	//END top-right

	//top and bottom rows
	for(int c = 2; c < cols - 2; c += 2) {
		//top row
		gVal = bVal = rVal = 0;
		for(int r = 0; r < 3; r++) {
			gVal += ((tRows[r][channels * (c - 2)] + tRows[r][channels * (c + 2)] +
				      (tRows[r][channels * (c - 1)] + tRows[r][channels * (c + 1)]) * 4 +
					    tRows[r][channels * c] * 6) * kernel[r + 2]);
			bVal += ((tRows[r][channels * (c - 2) + 1] + tRows[r][channels * (c + 2) + 1] +
				      (tRows[r][channels * (c - 1) + 1] + tRows[r][channels * (c + 1) + 1]) * 4 +
					    tRows[r][channels * c + 1] * 6) * kernel[r + 2]);
			rVal += ((tRows[r][channels * (c - 2) + 2] + tRows[r][channels * (c + 2) + 2] +
				      (tRows[r][channels * (c - 1) + 2] + tRows[r][channels * (c + 1) + 2]) * 4 +
					    tRows[r][channels * c + 2] * 6) * kernel[r + 2]);
		}
		top[channels * (c/2)] = gVal / 176;
		top[channels * (c/2) + 1] = bVal / 176;
		top[channels * (c/2) + 2] = rVal / 176;

		gVal = bVal = rVal = 0;

		for(int r = 0; r < 3 + rOffset; r++) {
			gVal += ((bRows[r][channels * (c - 2)] + bRows[r][channels * (c + 2)] +
					   (bRows[r][channels * (c - 1)] + bRows[r][channels * (c + 1)]) * 4 +
					    bRows[r][channels * c] * 6) * kernel[r]);
			bVal += ((bRows[r][channels * (c - 2) + 1] + bRows[r][channels * (c + 2) + 1] +
					   (bRows[r][channels * (c - 1) + 1] + bRows[r][channels * (c + 1) + 1]) * 4 +
					    bRows[r][channels * c + 1] * 6) * kernel[r]);
			rVal += ((bRows[r][channels * (c - 2) + 2] + bRows[r][channels * (c + 2) + 2] +
					   (bRows[r][channels * (c - 1) + 2] + bRows[r][channels * (c + 1) + 2]) * 4 +
					    bRows[r][channels * c + 2] * 6) * kernel[r]);
		}
		bottom[channels * (c/2)] = gVal / rDiv;
		bottom[channels * (c/2) + 1] = bVal / rDiv;
		bottom[channels * (c/2) + 2] = rVal / rDiv;
	} //END top and bottom rows


	//sides
	double* nRows[5];
	for(int r = 2; r < rows - 2; r += 2) {
		gVal = bVal = rVal = 0;
		// get row pointers
		nRows[0] = src.ptr<double>(r - 2);
		nRows[1] = src.ptr<double>(r - 1);
		nRows[2] = src.ptr<double>(r);
		nRows[3] = src.ptr<double>(r + 1);
		nRows[4] = src.ptr<double>(r + 2);
		double* target = dst.ptr<double>(r/2);

		//FIRST COLUMN
		for(int i = 0; i < 5; i++) {
			gVal += (nRows[i][0] * 6 +
				     nRows[i][3] * 4 +
					  nRows[i][6]) * kernel[i];
			bVal += (nRows[i][1] * 6 +
					  nRows[i][4] * 4 +
					  nRows[i][7]) * kernel[i];
			rVal += (nRows[i][2] * 6 +
					  nRows[i][5] * 4 +
					  nRows[i][8]) * kernel[i];
		}
		target[0] = gVal / 176;
		target[1] = bVal / 176;
		target[2] = rVal / 176;

		gVal = bVal = rVal = 0;

		//LAST COLUMN
		if(cOffset) {
			for(int i = 0; i < 5; i++) {
				gVal += (nRows[i][channels * (cols - 4)] * 1 +
						  (nRows[i][channels * (cols - 3)] + nRows[i][channels * (cols - 1)]) * 4 +
						  nRows[i][channels * (cols - 2)] * 6) * kernel[i];
				bVal += (nRows[i][channels * (cols - 4) + 1] * 1 +
						  (nRows[i][channels * (cols - 3) + 1] + nRows[i][channels * (cols - 1) + 1]) * 4 +
						  nRows[i][channels * (cols - 2) + 1] * 6) * kernel[i];
				rVal += (nRows[i][channels * (cols - 4) + 2] * 1 +
						  (nRows[i][channels * (cols - 3) + 2] + nRows[i][channels * (cols - 1) + 2]) * 4 +
						  nRows[i][channels * (cols - 2) + 2] * 6) * kernel[i];
			}
		} else {
			for(int i = 0; i < 5; i++) {
				gVal += (nRows[i][channels * (cols - 3)] * 1 +
						  (nRows[i][channels * (cols - 2)]) * 4 +
						  nRows[i][channels * (cols - 1)] * 6) * kernel[i];
				bVal += (nRows[i][channels * (cols - 3) + 1] * 1 +
						  (nRows[i][channels * (cols - 2) + 1]) * 4 +
						  nRows[i][channels * (cols - 1) + 1] * 6) * kernel[i];
				rVal += (nRows[i][channels * (cols - 3) + 2] * 1 +
						  (nRows[i][channels * (cols - 2) + 2]) * 4 +
						  nRows[i][channels * (cols - 1) + 2] * 6) * kernel[i];

			}
		}
		target[channels * (cols/2 - 1)] = gVal / cDiv;
		target[channels * (cols/2 - 1) + 1] = bVal / cDiv;
		target[channels * (cols/2 - 1) + 2] = rVal / cDiv;
	}

	//bottom-left corner
	gVal = bVal = rVal = 0;
	nRows[0] = src.ptr<double>(rows - 4);
	nRows[1] = src.ptr<double>(rows - 3);
	nRows[2] = src.ptr<double>(rows - 2);
	nRows[3] = src.ptr<double>(rows - 1);

	for(int i = 0; i < 3 + rOffset; i++) {
		gVal += ((nRows[i][0]) * 6 +
				  nRows[i][3] * 4 +
				  nRows[i][6]) * kernel[i];
		bVal += ((nRows[i][1]) * 6 +
				  nRows[i][4] * 4 +
				  nRows[i][7]) * kernel[i];
		rVal += ((nRows[i][2]) * 6 +
				  nRows[i][5] * 4 +
				  nRows[i][8]) * kernel[i];
	}
	bottom[0] = gVal / blDiv;
	bottom[1] = bVal / blDiv;
	bottom[2] = rVal / blDiv;	

	gVal = bVal = rVal = 0;

	//bottom right corner
	//use rows that were already set up
	for(int r = 0; r < 3 + rOffset; r++) {
		for(int c = 0; c < 3 + cOffset; c++) {
			int weight = kernel[r] * kernel[c];
			gVal += nRows[r][channels * (cols - (3 + cOffset) + c)] * weight;
			bVal += nRows[r][channels * (cols - (3 + cOffset) + c) + 1] * weight;
			rVal += nRows[r][channels * (cols - (3 + cOffset) + c) + 2] * weight;
		}
	}
	bottom[channels * ((cols/2) - 1)] = gVal / brDiv;
	bottom[channels * ((cols/2) - 1) + 1] = bVal / brDiv;
	bottom[channels * ((cols/2) - 1) + 2] = rVal / brDiv;
}

/**********************************************************
 *
 *********************************************************/
void Recorder::runAmp() {
	std::unique_lock<std::mutex> lk(m[this->mThreadIdx]);
	if (Running[this->mThreadIdx]) {
		//only wait on the condition variable if thread already exists
		//and can signal.
		while (Ready[this->mThreadIdx]) {
			cnd[this->mThreadIdx].wait(lk);
		}
	} else {
		//spin off the thread
		Threads[this->mThreadIdx] = new std::thread([&]{
			this->mAmps[this->mThreadIdx]->run();
		});

		// Wait until the thread has been created before continuing
		while(!Running[this->mThreadIdx]) {
			cnd[this->mThreadIdx].wait(lk);
		}
	}
	Ready[this->mThreadIdx] = true;
	if (lk.owns_lock()) {
		lk.unlock();
	}
	//lock is only necessary for waiting; signalling can still be done.
	cnd[this->mThreadIdx].notify_all();
	this->mThreadIdx = (this->mThreadIdx + 1) % T_COUNT;
}

/**********************************************************
 *
 *********************************************************/
void Recorder::onAmpSlider(int val, void* userData) {
	Recorder::mParams->setAmpLevel(val / 10);
}

/**********************************************************
 *
 *********************************************************/
void Recorder::onFilterSlider(int val, void* userData) {
	if(!Recorder::mParams->setFilter(val)) {
		cvSetTrackbarPos("Filter", "EVM", 0); //default to gaussian
		Recorder::mParams->setFilter(0);
	}
}

/**********************************************************
 *
 *********************************************************/
void Recorder::onHighSlider(int val, void* userData) {
	if(!Recorder::mParams->setHighLevel(val)) {
		cvSetTrackbarPos("High", "EVM", Recorder::mParams->getLowLevel());
	}
	Recorder::mParams->setHighLevel(Recorder::mHighLevel);
}

/**********************************************************
 *
 *********************************************************/
void Recorder::onLowSlider(int val, void* userData) {
	if(!Recorder::mParams->setLowLevel(val)) {
		cvSetTrackbarPos("Low", "EVM", Recorder::mParams->getHighLevel());
	}
	Recorder::mParams->setLowLevel(Recorder::mLowLevel);
}
