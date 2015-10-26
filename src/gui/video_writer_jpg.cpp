#include "video_writer_jpg.hpp"
#include "basic_types.hpp"
#include "exception.hpp"
#include <iostream>
#include <QFileInfo>
#include <QThreadPool>
#include <stdexcept>
#include <opencv2/highgui/highgui.hpp>
#include <vector>
#include <QtDebug>

namespace bias
{

    const QString VideoWriter_jpg::IMAGE_FILE_BASE = QString("image_");
    const QString VideoWriter_jpg::IMAGE_FILE_EXT = QString(".jpg");
    const QString DUMMY_FILENAME("dummy.jpg");
    const unsigned int VideoWriter_jpg::FRAMES_TODO_MAX_QUEUE_SIZE = 250;
    const unsigned int VideoWriter_jpg::DEFAULT_FRAME_SKIP = 1;
    const unsigned int VideoWriter_jpg::DEFAULT_QUALITY = 90;
    const unsigned int VideoWriter_jpg::MIN_QUALITY = 0;
    const unsigned int VideoWriter_jpg::MAX_QUALITY = 100;
    const unsigned int VideoWriter_jpg::DEFAULT_NUMBER_OF_COMPRESSORS = 10;
    const VideoWriterParams_jpg VideoWriter_jpg::DEFAULT_PARAMS = VideoWriterParams_jpg();

    // VideoWriter_jpg methods
    VideoWriter_jpg::VideoWriter_jpg(QObject *parent) : VideoWriter_jpg(DEFAULT_PARAMS,DUMMY_FILENAME,0,parent) 
    {}

    VideoWriter_jpg::VideoWriter_jpg(
            VideoWriterParams_jpg params,
            QString fileName, 
            unsigned int cameraNumber,
            QObject *parent
            ) : VideoWriter(fileName,cameraNumber,parent) 
    {

        std::cout << params.toString() << std::endl;

        isFirst_ = true;
        setFrameSkip(params.frameSkip);
        quality_ = params.quality;
        numberOfCompressors_ = params.numberOfCompressors; 

        threadPoolPtr_ = new QThreadPool(this);
        threadPoolPtr_ -> setMaxThreadCount(numberOfCompressors_);
        framesToDoQueuePtr_ = std::make_shared<CompressedFrameQueue_jpg>();
    }

    VideoWriter_jpg::~VideoWriter_jpg() 
    {
        stopCompressors();
    }


    void VideoWriter_jpg::setFileName(QString fileName)
    {
        fileName_ = fileName;
        QFileInfo fileInfo(fileName_);
        baseName_ = fileInfo.baseName();
        baseDir_ = fileInfo.dir();
    }

    void VideoWriter_jpg::addFrame(StampedImage stampedImg)
    {
        if (isFirst_)
        {
            setupOutput();
            startCompressors();
            isFirst_= false;
        }

        QString imageFileName = IMAGE_FILE_BASE;  
        imageFileName += QString::number(frameCount_);
        imageFileName += IMAGE_FILE_EXT;
        QFileInfo imageFileInfo(logDir_,imageFileName);
        QString fullPathName = imageFileInfo.absoluteFilePath();

        unsigned int framesToDoQueueSize = framesToDoQueuePtr_ -> size();

        if (frameCount_%frameSkip_==0) 
        {
            CompressedFrame_jpg compressedFrame(fullPathName,stampedImg,quality_);

            framesToDoQueuePtr_ -> acquireLock();
            framesToDoQueuePtr_ -> push(compressedFrame);
            framesToDoQueuePtr_ -> wakeOne();
            framesToDoQueueSize = framesToDoQueuePtr_ -> size();
            framesToDoQueuePtr_ -> releaseLock();
            std::cout << "size = " << framesToDoQueueSize << std::endl;
        }
        if (framesToDoQueueSize > FRAMES_TODO_MAX_QUEUE_SIZE) 
        { 
            std::cout << "error: framesToDoQueueSize = " << framesToDoQueueSize << std::endl;
            unsigned int errorId = ERROR_FRAMES_TODO_MAX_QUEUE_SIZE;
            QString errorMsg("logger framesToDoQueue has exceeded the maximum allowed size");
            emit imageLoggingError(errorId, errorMsg);
        }

        frameCount_++;
    }


    void VideoWriter_jpg::finish()
    {
        bool finished = false;
        while (!finished)
        {
            framesToDoQueuePtr_ -> acquireLock();
            if ( (framesToDoQueuePtr_ -> size()) == 0)
            {
                finished = true;
            }
            framesToDoQueuePtr_ -> releaseLock();
        }
    }

    unsigned int VideoWriter_jpg::getNextVersionNumber()
    {
        unsigned int nextVerNum = 0;
        if (addVersionNumber_)
        {
            bool done = false;
            QDir testLogDir;
            while (!done)
            {
                nextVerNum++;
                testLogDir = getLogDir(nextVerNum);
                if (!testLogDir.exists())
                {
                    done = true;
                }
            }
        }
        return nextVerNum;
    }

    void VideoWriter_jpg::setupOutput()
    {
        if (!baseDir_.exists())
        {
            unsigned int errorId = ERROR_VIDEO_WRITER_INITIALIZE;
            std::string errorMsg("base log directory, "); 
            errorMsg += baseDir_.absolutePath().toStdString();
            errorMsg += std::string("does not exist");
            throw RuntimeError(errorId, errorMsg);
        }

        unsigned int verNum = getNextVersionNumber();
        QString logDirName = getLogDirName(verNum);
        logDir_ = getLogDir(verNum);

        if (baseDir_.exists(logDirName) && !addVersionNumber_)
        {
            QString dirCopyName = logDirName + QString("_copy");
            if (baseDir_.exists(dirCopyName))
            {
                bool done = false;
                unsigned int cnt = 2;
                while (!done)
                {
                    dirCopyName = logDirName + QString("_copy_%1").arg(cnt);
                    if (!baseDir_.exists(dirCopyName))
                    {
                        done = true;
                    }
                    cnt++;
                }
            }
            baseDir_.rename(logDirName,  dirCopyName);
        }

        if (!baseDir_.mkdir(logDirName))
        {
            unsigned int errorId = ERROR_VIDEO_WRITER_INITIALIZE;
            std::string errorMsg("unable to create log directory, "); 
            errorMsg += baseDir_.absolutePath().toStdString();
            throw RuntimeError(errorId, errorMsg);
        }

    }

    QString VideoWriter_jpg::getUniqueDirName()
    {
        unsigned int nextVerNum = getNextVersionNumber();
        QString logDirName = getLogDirName(nextVerNum);
        return logDirName;
    }

    QString VideoWriter_jpg::getLogDirName(unsigned int verNum)
    {
        QString logDirName;
        if (verNum > 0)
        { 
            QString verStr = QString("_v%1").arg(verNum,3,10,QChar('0'));
            logDirName =  baseName_ + verStr;
        }
        else
        {
            logDirName = baseName_;
        }
        return logDirName;
    }

    QDir VideoWriter_jpg::getLogDir(unsigned int verNum)
    {
        QString logDirName = getLogDirName(verNum);
        QDir logDir = QDir(baseDir_.absolutePath() + "/" + logDirName);
        return logDir;
    }


    void VideoWriter_jpg::startCompressors()
    {
        framesToDoQueuePtr_ -> clear();
        compressorPtrVec_.resize(numberOfCompressors_);
        for (unsigned int i=0; i<compressorPtrVec_.size(); i++)
        {
            compressorPtrVec_[i] = new Compressor_jpg(framesToDoQueuePtr_, cameraNumber_);
            threadPoolPtr_ -> start(compressorPtrVec_[i]);
        }
    }


    void VideoWriter_jpg::stopCompressors()
    {
        // Send all compressor threads a stop signal 
        for (unsigned int i=0; i<compressorPtrVec_.size(); i++)
        {
            if (!(compressorPtrVec_[i].isNull()))
            { 
                compressorPtrVec_[i] -> acquireLock();
                compressorPtrVec_[i] -> stop();
                compressorPtrVec_[i] -> releaseLock();
            }
        }

        // Wait until all compressor threads are null
        for (unsigned int i=0; i<compressorPtrVec_.size(); i++)
        {
            while (!(compressorPtrVec_[i].isNull()))
            {
                framesToDoQueuePtr_ -> acquireLock();
                framesToDoQueuePtr_ -> wakeOne();
                framesToDoQueuePtr_ -> releaseLock();
            }
        }
    }

} // namespace bias