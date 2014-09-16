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
   time_t now, before;
   time(&now);
   before = now;
   while (1) {
      this->displayFrame();
      if (frameCount >= BUF_SIZE / 4) {
         frameCount = 0;
         this->runAmp();
         time(&now);
         std::cout << "FPS: "
                   << (BUF_SIZE / 4) / difftime(now, before) 
                   << std::endl;
         before = now;
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
   if(!this->mClean[this->mCursor].empty() && !this->mAmplified[this->mCursor].empty()) {
      cv::imshow("Clean", this->mClean[this->mCursor]);
      cv::imshow("EVM", this->mAmplified[this->mCursor]);
   }
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

      //TODO: shrink the image, store in process buffer
      frame.convertTo(frame, CV_64F);
      bool filter = Recorder::mParams->getFilter();
      for (int l = 0; l < PYR_LEVELS; l++) {
         if(filter) {
            //Laplacian
            cv::Mat tmp = Amplifier::pyrDown(frame);
            tmp = Amplifier::pyrUp(tmp);
            int rows = frame.rows;
            int width = frame.cols * frame.channels();
            for (int r = 0; r < rows; r++) {
               double* data1 = frame.ptr<double>(r);
               double* data2 = tmp.ptr<double>(r);
               for (int w = 0; w < width; w++) {
                  data1[w] = (data1[w] - data2[w]) * 5;
               }
            }
         } else {
            usleep(10000);
         }
         //Gaussian (or finish last step of Laplacian)
         frame = Amplifier::pyrDown(frame);
      }
      frame.copyTo(this->mReduced[this->mCursor]);

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
      this->mAmps[i] = new Amplifier(i, this->mReduced, this->mAmplified, this->mClean);
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
