#include "compressed_frame_ufmf.hpp"
#include <algorithm>
#include <iostream>

namespace bias
{
    // Constants 
    // ------------------------------------------------------------------------------
    const uchar CompressedFrame_ufmf::BACKGROUND_MEMBER_VALUE = 255; 
    const uchar CompressedFrame_ufmf::FOREGROUND_MEMBER_VALUE = 0;
    const unsigned int CompressedFrame_ufmf::DEFAULT_BOX_LENGTH = 30; 
    const double CompressedFrame_ufmf::DEFAULT_FG_MAX_FRAC_COMPRESS = 1.0;


    // Methods
    // -------------------------------------------------------------------------------
    CompressedFrame_ufmf::CompressedFrame_ufmf() 
        : CompressedFrame_ufmf(DEFAULT_BOX_LENGTH,DEFAULT_FG_MAX_FRAC_COMPRESS) 
    { }


    CompressedFrame_ufmf::CompressedFrame_ufmf(
            unsigned int boxLength, 
            double fgMaxFracCompress
            ) 
    {
        haveData_ = false;
        isCompressed_ = false;
        ready_ = false;
        numPix_ = 0;
        numForeground_ = 0;
        numPixWritten_ = 0;
        numConnectedComp_ = 0;
        boxLength_ = boxLength;
        boxArea_ = boxLength*boxLength;
        fgMaxFracCompress_ = fgMaxFracCompress;
    }


    bool CompressedFrame_ufmf::haveData() const
    {
        return haveData_;
    }


    unsigned long CompressedFrame_ufmf::getFrameCount() const
    {
        unsigned long frameCount;

        if (haveData_)
        {
            frameCount = stampedImg_.frameCount;
        }
        else 
        {
            frameCount = 0;
        }
        return frameCount;
    }


    void CompressedFrame_ufmf::setData(
            StampedImage stampedImg, 
            cv::Mat bgLowerBound,
            cv::Mat bgUpperBound
            )
    {

        // Save original stamped image
        stampedImg_ = stampedImg;
        bgLowerBound_ = bgLowerBound;
        bgUpperBound_ = bgUpperBound;
        haveData_ = true;
    }


    void CompressedFrame_ufmf::compress()
    {

        // ---------------------------------------------------------------------
        // NOTE, probably should raise an exception here
        // ---------------------------------------------------------------------
        if (!haveData_) { return; }

        // Get number of rows, cols and pixels from image
        unsigned int numRow = (unsigned int) (stampedImg_.image.rows);
        unsigned int numCol = (unsigned int) (stampedImg_.image.cols);
        unsigned int numPix = numRow*numCol;

        // Allocate memory for compressed frames if required and set buffer values
        if (numPix_ != numPix) 
        {
            numPix_ = numPix;
            allocateBuffers();
        }
        resetBuffers();

        unsigned int fgMaxNumCompress = (unsigned int)(double(numPix)*fgMaxFracCompress_);

        // Get background/foreground membership, 255=background, 0=foreground
        cv::inRange(stampedImg_.image, bgLowerBound_, bgUpperBound_, membershipImage_);
        numForeground_ = numPix - cv::countNonZero(membershipImage_);

        numConnectedComp_ = 0;
        numPixWritten_ = 0;

        // Create frame - uncompressed/compressed based on number of foreground pixels
        if (numForeground_ > fgMaxNumCompress)
        {
            createUncompressedFrame();
        }
        else 
        {
            createCompressedFrame();
        }
        ready_ = true;

    } // CompressedFrame_ufmf::compress


    void CompressedFrame_ufmf::createUncompressedFrame()
    { 
        unsigned int numRow = (unsigned int) (stampedImg_.image.rows);
        unsigned int numCol = (unsigned int) (stampedImg_.image.cols);

        writeRowBuf_[0] = 0;
        writeColBuf_[0] = 0;
        writeHgtBuf_[0] = numRow;
        writeWdtBuf_[0] = numCol;

        unsigned int pixCnt = 0;
        for (unsigned int row=0; row<numRow; row++)
        {
            for (unsigned int col=0; col<numCol; col++)
            {
                imageDatBuf_[pixCnt] = (uint8_t) stampedImg_.image.at<uchar>(row,col);
                numWriteBuf_[pixCnt] = 1;
                pixCnt++;
            }
        }
        numPixWritten_ = numRow*numCol; 
        numConnectedComp_ = 1;
        isCompressed_ = false;

    } // CompressedFrame_ufmf::createUncompressedFrame


    void CompressedFrame_ufmf::createCompressedFrame()
    { 
        // Get number of rows, cols and pixels from image
        unsigned int numRow = (unsigned int) (stampedImg_.image.rows);
        unsigned int numCol = (unsigned int) (stampedImg_.image.cols);
        unsigned int numPix = numRow*numCol;
        unsigned int imageDatInd = 0;

        for (unsigned int row=0; row<numRow; row++)
        {
            for (unsigned int col=0; col<numCol; col++)
            {
                // Start new box if pixel if foreground or continue to next pixel
                if (membershipImage_.at<uchar>(row,col) == BACKGROUND_MEMBER_VALUE) 
                { 
                    continue;
                }

                // store everything in box with corner at (row,col)
                writeRowBuf_[numConnectedComp_] = row;
                writeColBuf_[numConnectedComp_] = col;
                writeHgtBuf_[numConnectedComp_] = std::min(boxLength_, numRow-row);
                writeWdtBuf_[numConnectedComp_] = std::min(boxLength_, numCol-col);

                // Loop through pixels to store
                unsigned int rowPlusHeight = row + writeHgtBuf_[numConnectedComp_];

                for (unsigned int rowEnd=row; rowEnd < rowPlusHeight; rowEnd++)
                {
                    bool stopEarly = false;
                    unsigned int colEnd = col;
                    unsigned int numWriteInd = rowEnd*numCol + col;
                    unsigned int colPlusWidth = col + writeWdtBuf_[numConnectedComp_];

                    // Check if we've already written something in this column
                    for (colEnd=col; colEnd < colPlusWidth; colEnd++)
                    {
                        if (numWriteBuf_[numWriteInd] > 0)
                        {
                            stopEarly = true;
                            break;
                        }
                        numWriteInd += 1;

                    }  // for (unsigned int colEnd 

                    if (stopEarly) 
                    {
                        if (rowEnd == row)
                        { 
                            // If this is the first row - shorten the width and write as usual
                            writeWdtBuf_[numConnectedComp_] = colEnd - col;
                        }
                        else
                        {
                            // Otherwise, shorten the height, and don't write any of this row
                            writeHgtBuf_[numConnectedComp_] = rowEnd - row;
                            break;
                        }

                    } // if (stopEarly) 

                    colPlusWidth = col + writeWdtBuf_[numConnectedComp_];
                    numWriteInd = rowEnd*numCol + col;

                    for (colEnd=col; colEnd < colPlusWidth; colEnd++)
                    {
                        numWriteBuf_[numWriteInd] += 1;
                        numWriteInd += 1;
                        imageDatBuf_[imageDatInd] = (uint8_t) stampedImg_.image.at<uchar>(rowEnd,colEnd); 
                        imageDatInd += 1;
                        membershipImage_.at<uchar>(rowEnd,colEnd) = BACKGROUND_MEMBER_VALUE;
                    } // for (unsigned int colEnd 


                } // for (unsigned int rowEnd 

                numConnectedComp_++;

            } // for (unsigned int col

        } // for (unsigned int row

        isCompressed_ = true;

        std::cout << "numConnectedComp: " << numConnectedComp_ << std::endl;

    } // CompressedFrame_ufmf::createCompressedFrame


    //cv::Mat CompressedFrame_ufmf::getMembershipImage()
    //{
    //    return membershipImage_;
    //}


    void CompressedFrame_ufmf::allocateBuffers()
    {
        writeRowBuf_.resize(numPix_);
        writeColBuf_.resize(numPix_);
        writeHgtBuf_.resize(numPix_);
        writeWdtBuf_.resize(numPix_);
        numWriteBuf_.resize(numPix_);
        imageDatBuf_.resize(numPix_);
        
    }


    void CompressedFrame_ufmf::resetBuffers()
    {
        std::fill_n(writeRowBuf_.begin(), numPix_, 0); 
        std::fill_n(writeColBuf_.begin(), numPix_, 0);
        std::fill_n(writeHgtBuf_.begin(), numPix_, 0);
        std::fill_n(writeWdtBuf_.begin(), numPix_, 0);
        std::fill_n(numWriteBuf_.begin(), numPix_, 0);
        std::fill_n(imageDatBuf_.begin(), numPix_, 0);
    }


    // Compressed frame comparison operator
    // ----------------------------------------------------------------------------------------
    bool CompressedFrameCmp_ufmf::operator() (
            const CompressedFrame_ufmf &cmpFrame0,
            const CompressedFrame_ufmf &cmpFrame1
            )
    {
        bool haveData0 = cmpFrame0.haveData();
        bool haveData1 = cmpFrame1.haveData();

        if ((haveData0 == false) && (haveData1 == false))
        {
            return false;
        }
        else if ((haveData0 == false) && (haveData1 == true))
        {
            return false;
        }
        else if ((haveData0 == true) && (haveData1 == false))
        {
            return true;
        }
        else
        {
            unsigned long frameCount0 = cmpFrame0.getFrameCount();
            unsigned long frameCount1 = cmpFrame1.getFrameCount();
            return (frameCount0 < frameCount1);
        }
    }

} // namespace bias