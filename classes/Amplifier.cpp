/******************************************************************************
 * FILE: Amplifier.cpp
 *****************************************************************************/

#include "Amplifier.hpp"

Params* Amplifier::mParams = Params::getInstance();

/**********************************************************
 *
 *********************************************************/
Amplifier::Amplifier(int tid, cv::Mat* pSrc, cv::Mat* pDst, cv::Mat* pOrig) {
   this->mCpyCursor = ((BUF_SIZE / 4) * (tid + 2)) % BUF_SIZE;
   this->mProcCursor = ((BUF_SIZE / 4) * tid) % BUF_SIZE;
   this->mSrc = pSrc;
   this->mDst = pDst;
   this->mOrig = pOrig;
   this->mTid = tid;

   this->getAllFrames();
}

/**********************************************************
 *
 *********************************************************/
Amplifier::~Amplifier() {
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::run() {
   //main loop
   std::unique_lock<std::mutex> lk(m[this->mTid]);
   // Signal that this thread has been successfully created.
   Running[this->mTid] = true;
   bool full = false;
   cnd[this->mTid].notify_all();

   while (1) {
      //1: wait on condition variable
      while(!Ready[this->mTid]) {
         cnd[this->mTid].wait(lk);
      }
      //2: get new frames
      this->getNewFrames();
      //3: init process buffer
      this->initProcessBuffer();
      //4: bandpass filter
      if(full) {
         this->bandpassFilter();
         //5: amplify/scale
         this->scaleAndAmplify();
         //6: merge new frames back in
         this->putNewFrames();
      } else {
         full = true;
         for(int i = 0; i < BUF_SIZE * 2; i++) {
            if(this->mProcess[i].empty()) {
               full = false;
               break;
            }
         }
      }

      //7: release the lock
      Ready[this->mTid] = false;
      if (lk.owns_lock()) {
         lk.unlock();
      }
      cnd[this->mTid].notify_all();
   }
}


/**********************************************************
 *
 *********************************************************/
cv::Mat Amplifier::pyrUp(cv::Mat &src) {
   int tRows = src.rows * 2;
   int cols = src.cols * 2;
   int channels = src.channels();
   cv::Mat dst(cv::Size(cols, tRows), src.type());
   Amplifier::pyrUpEdges(src, dst);
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
void Amplifier::pyrUpEdges(cv::Mat &src, cv::Mat &dst) {
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
cv::Mat Amplifier::pyrDown(cv::Mat &src) {
   int rows = src.rows;
   int cols = src.cols;
   int channels = src.channels();
   cv::Mat dst(cv::Size(cols/2, rows/2), src.type());
   double kernel[5] = {1.0, 4.0, 6.0, 4.0, 1.0};

   Amplifier::pyrDownEdges(src, dst);

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
void Amplifier::pyrDownEdges(cv::Mat &src, cv::Mat &dst) {
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
void Amplifier::bandpassFilter() {
   //get the frequency domain into the process buffer
   this->fft();

   //NOTE: the sliders won't represent actual frequency values;
   //instead, they will represent a selected index in the fft
   //array
   int min = this->mParams->getLowLevel();
   int max = this->mParams->getHighLevel();

   //flatten the frequencies
   for(int i = 0; i < min; i++)
   {
      this->mProcess[i].setTo(0);
   }
   for(int i = max + 1; i < (BUF_SIZE * 2) - max; i++)
   {
      this->mProcess[i].setTo(0);
   }
   for(int i = (BUF_SIZE * 2) - min; i < (BUF_SIZE * 2); i++)
   {
      this->mProcess[i].setTo(0);
   }

   //undo the fft to get the buffer back
   this->ifft();
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::getNewFrames() {
   for (int i = 0; i < ((3 * BUF_SIZE) / 4); i++) {
      this->mSrc[this->mCpyCursor].copyTo(this->mClean[this->mCpyCursor]);
      this->mCpyCursor = (this->mCpyCursor + 1) % BUF_SIZE;
   }
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::getAllFrames() {
   for (int i = 0; i < BUF_SIZE; i++) {
      this->mSrc[i].copyTo(this->mClean[i]);
   }
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::putNewFrames() {
   //TODO: recombine with original
   int idx = this->mProcCursor;
   int typeTo = this->mDst[0].type();
   int typeFrom = this->mProcess[0].type();
   int rows = this->mOrig[idx].rows;
   int width = this->mOrig[idx].cols * this->mOrig[idx].channels();

   for (int i = ((3 * BUF_SIZE) / 4); i < BUF_SIZE; i++) {
      //Undo the pyramids, resizing frames to original size
      for (int l = 0; l < PYR_LEVELS; l++) {
         this->mProcess[2 * i] = Amplifier::pyrUp(this->mProcess[2 * i]);
      }
      cv::Mat tmp;
      this->mOrig[idx].convertTo(tmp, typeFrom);
      //Combine with originals
      for (int r = 0; r < rows; r++) {
         double* orig = tmp.ptr<double>(r);
         double* amped = this->mProcess[2 * i].ptr<double>(r);
         for (int c = 0; c < width; c++) {
            orig[c] += amped[c];
         }
      }
      tmp.convertTo(this->mDst[idx], typeTo);
      idx = (idx + 1) % BUF_SIZE;
   }
   this->mProcCursor = (idx + (BUF_SIZE / 2)) % BUF_SIZE;
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::initProcessBuffer() {
   int idx = this->mCpyCursor;
   int rows = this->mClean[idx].rows;
   int cols = this->mClean[idx].cols;
   int type = this->mClean[idx].type();
   for (int i = 0; i < BUF_SIZE; i++) {
      this->mClean[idx].copyTo(this->mProcess[2 * i]);
      this->mProcess[(2 * i) + 1] = cv::Mat::zeros(rows, cols, CV_64FC4);
      idx = (idx + 1) % BUF_SIZE;
   }
}

/**********************************************************
 * 
 *********************************************************/
void Amplifier::invertIdx() {
   //NOTE: Assumes buffer size = BUFFER_SIZE
   static int lookup[BUF_SIZE] = {0};
   if (lookup[1] == 0)
   {
      //set up the lookup table only once
      switch(BUF_SIZE)
      {
         case 16:
            {
               int tmp[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11,
                  7, 15};
               for (int i = 0; i < BUF_SIZE; i++)
               {
                  lookup[i] = tmp[i];
               }
               break;
            }
         case 32:
            {
               int tmp[32] = {0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22,
                  14, 30, 1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27,
                  7, 23, 15, 31};
               for (int i = 0; i < BUF_SIZE; i++)
               {
                  lookup[i] = tmp[i];
               }
               break;
            }
         case 64:
            {
               int tmp[64] = {0, 32, 16, 48, 8, 40, 24, 56, 4, 36, 20, 52, 12,
                  44, 28, 60, 2, 34, 18, 50, 10, 42, 26, 58, 6, 38,
                  22, 54, 14, 46, 30, 62, 1, 33, 17, 49, 9, 41, 25, 
                  57, 5, 37, 21, 53, 13, 45, 29, 61, 3, 35, 19, 51, 
                  11, 43, 27, 59, 7, 39, 23, 55, 15, 47, 31, 63};
               for (int i = 0; i < BUF_SIZE; i++)
               {
                  lookup[i] = tmp[i];
               }
               break;
            }
         case 128:
            {
               int tmp[128] = {0, 64, 32, 96, 16, 80, 48, 112, 8, 72, 40, 104,
                  24, 88, 56, 120, 4, 68, 36, 100, 20, 84, 52,
                  116, 12, 76, 44, 108, 28, 92, 60, 124, 2, 66,
                  34, 98, 18, 82, 50, 114, 10, 74, 42, 106, 26,
                  90, 58, 122, 6, 70, 38, 102, 22, 86, 54, 118,
                  14, 78, 46, 110, 30, 94, 62, 126, 1, 65, 33,
                  97, 17, 81, 49, 113, 9, 73, 41, 105, 25, 89,
                  57, 121, 5, 69, 37, 101, 21, 85, 53, 117, 13,
                  77, 45, 109, 29, 93, 61, 125, 3, 67, 35, 99,
                  19, 83, 51, 115, 11, 75, 43, 107, 27, 91, 59,
                  123, 7, 71, 39, 103, 23, 87, 55, 119, 15, 79,
                  47, 111, 31, 95, 63, 127};
               for (int i = 0; i < BUF_SIZE; i++)
               {
                  lookup[i] = tmp[i];
               }
               break;
            }
         case 256:
            {
               int tmp[256] = {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80,
                  208, 48, 176, 112, 240, 8, 13, 72, 200, 40, 168,
                  104, 232, 24, 152, 88, 216, 56, 184, 120, 248, 4,
                  132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212,
                  52, 180, 116, 244, 12, 140, 76, 204, 44, 172, 108,
                  236, 28, 156, 92, 220, 60, 188, 124, 252, 2, 130,
                  66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50,
                  178, 114, 242, 10, 138, 74, 202, 42, 170, 106, 234,
                  26, 154, 90, 218, 58, 186, 122, 250, 6, 134, 70,
                  198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182,
                  118, 246, 14, 142, 78, 206, 46, 174, 110, 238, 30,
                  158, 94, 222, 62, 190, 126, 254, 1, 129, 65, 193,
                  33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113,
                  241, 9, 137, 73, 201, 41, 169, 105, 233, 25, 153,
                  89, 217, 57, 185, 121, 249, 5, 133, 69, 197, 37,
                  165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
                  13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93,
                  221, 61, 189, 125, 253, 3, 131, 67, 195, 35, 163,
                  99, 227, 19, 147, 83, 211, 51, 179, 115, 243, 11,
                  139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219,
                  59, 187, 123, 251, 7, 135, 71, 199, 39, 167, 103,
                  231, 23, 151, 87, 215, 55, 183, 119, 247, 15, 143,
                  79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63,
                  191, 127, 255};
               for (int i = 0; i < BUF_SIZE; i++)
               {
                  lookup[i] = tmp[i];
               }
               break;
            }
         default:
            {
               std::cerr << "ERROR: invalud buffer size. Must be small power of 2.\n";
               exit(-1);
            }
      }
   }
   for (int i = 0; i < BUF_SIZE; i++)
   {
      //swap the elements
      if(i < lookup[i])
      {
         cv::Mat tmp = this->mProcess[2 * i];
         this->mProcess[2 * i] = this->mProcess[2 * lookup[i]];
         this->mProcess[2 * lookup[i]] = tmp;
         tmp = this->mProcess[(2 * i) + 1];
         this->mProcess[(2 * i) + 1] = this->mProcess[(2 * lookup[i]) + 1];
         this->mProcess[(2 * lookup[i]) + 1] = tmp;
      }
   }
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::fft() {
   this->invertIdx();

   unsigned long n, mmax, j, istep;
   double wtemp, wReal, wpReal, wpImaginary, wImaginary, theta;
   double tempReal, tempImaginary;
   unsigned int rows = this->mProcess[0].rows;
   unsigned int width = this->mProcess[0].cols * this->mProcess[0].channels();

   n = BUF_SIZE << 1;
   mmax = 2;
   while (n > mmax) {
      istep = mmax<<1;
      theta = -(2 * M_PI/mmax);
      wtemp = sin(0.5 * theta);
      wpReal = -2.0 * wtemp * wtemp;
      wpImaginary = sin(theta);
      wReal = 1.0;
      wImaginary = 0.0;
      for (unsigned long m = 1; m < mmax; m += 2) {
         for (unsigned long i = m; i <= n; i += istep) {
            j = i + mmax;
            //loop through each row
            for (int r = 0; r < rows; r++) {
               double* dataJ = this->mProcess[j].ptr<double>(r);
               double* dataJ1 = this->mProcess[j - 1].ptr<double>(r);
               double* dataI = this->mProcess[i].ptr<double>(r);
               double* dataI1 = this->mProcess[i - 1].ptr<double>(r);

               //loop through columns
               for (int w = 0; w < width; w++) {
                  tempReal = wReal * dataJ1[w] - wImaginary * dataJ[w];
                  tempImaginary = wReal * dataJ[w] + wImaginary * dataJ1[w];

                  dataJ1[w] = dataI1[w] - tempReal;
                  dataJ[w] = dataI[w] - tempImaginary;
                  dataI1[w] += tempReal;
                  dataI[w] += tempImaginary;
               }
            }
         }
         wtemp = wReal;
         wReal += wReal * wpReal - wImaginary * wpImaginary;
         wImaginary += wImaginary * wpReal + wtemp * wpImaginary;
      }
      mmax = istep;
   }
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::ifft() {
   this->invertIdx();

   unsigned long n, mmax, j, istep;
   double wtemp, wReal, wpReal, wpImaginary, wImaginary, theta;
   double tempReal, tempImaginary;
   unsigned int rows = this->mProcess[0].rows;
   unsigned int width = this->mProcess[0].cols * this->mProcess[0].channels();

   n = BUF_SIZE << 1;
   mmax = 2;
   while (n > mmax) {
      istep = mmax<<1;
      theta = -(2 * M_PI/mmax);
      wtemp = -sin(0.5 * theta);
      wpReal = -2.0 * wtemp * wtemp;
      wpImaginary = sin(theta);
      wReal = 1.0;
      wImaginary = 0.0;
      for (unsigned long m = 1; m < mmax; m += 2) {
         for (unsigned long i = m; i <= n; i += istep) {
            j = i + mmax;
            //loop through each row
            for (int r = 0; r < rows; r++) {
               double* dataJ = this->mProcess[j].ptr<double>(r);
               double* dataJ1 = this->mProcess[j - 1].ptr<double>(r);
               double* dataI = this->mProcess[i].ptr<double>(r);
               double* dataI1 = this->mProcess[i - 1].ptr<double>(r);

               //loop through columns
               for (int w = 0; w < width; w++) {
                  tempReal = wReal * dataJ1[w] - wImaginary * dataJ[w];
                  tempImaginary = wReal * dataJ[w] + wImaginary * dataJ1[w];

                  dataJ1[w] = dataI1[w] - tempReal;
                  dataJ[w] = dataI[w] - tempImaginary;
                  dataI1[w] += tempReal;
                  dataI[w] += tempImaginary;
               }
            }
         }
         wtemp = wReal;
         wReal += wReal * wpReal - wImaginary * wpImaginary;
         wImaginary += wImaginary * wpReal + wtemp * wpImaginary;
      }
      mmax = istep;
   }
}

/**********************************************************
 *
 *********************************************************/
void Amplifier::scaleAndAmplify() {
   //Scale:: for fft
   //Amplify:: for EVM
   //only amplify what needs to be amplified; the
   //last 1/4 of mProcess, only even indexes
   int idx = 3 * (BUF_SIZE / 4);

   int rows = this->mProcess[0].rows;
   int width = this->mProcess[0].cols * this->mProcess[0].channels();

   int ampFactor = Amplifier::mParams->getAmpLevel();

   while (idx < BUF_SIZE)
   {
      //loop through rows
      for (int r = 0; r < rows; r++)
      {
         //get row pointers
         double* row = this->mProcess[2 * idx].ptr<double>(r);
         for (int c = 0; c < width; c++)
         {
            //handle ifft scaling and amplification simultaneously
            row[c] *= ampFactor * 0.125;
         }
      }
      //advance through the buffer
      idx++;
   }
}
