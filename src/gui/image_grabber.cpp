#include "image_grabber.hpp"
#include "exception.hpp"
#include "camera.hpp"
#include "stamped_image.hpp"
#include "affinity.hpp"
#include <iostream>
#include <QTime>
#include <QThread>
#include <QFileInfo>
#include <opencv2/core/core.hpp>
#include "camera_device.hpp"


/*#include <cstdlib>
#include<thread>
#include<chrono>*/

// TEMPOERARY
// ----------------------------------------
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "win_time.hpp"
// ---------------------------------------

#include  <algorithm>

//#define DEBUG 1
//#define isVidInput 1
//#define isSkip 0

namespace bias {

    unsigned int ImageGrabber::DEFAULT_NUM_STARTUP_SKIP = 2;

    unsigned int ImageGrabber::MIN_STARTUP_SKIP = 2;
    unsigned int ImageGrabber::MAX_ERROR_COUNT = 500;
    unsigned int ImageGrabber::DEFAULT_TIMING_BUFFER_SIZE = 250; // future add this to nidaq config
	//string input_video_dir = "Y:/hantman_data/jab_experiments/STA14/STA14/20230503/STA14_20230503_142341/";

    ImageGrabber::ImageGrabber(QObject *parent) : QObject(parent)
    {
        
        initialize(0, NULL, NULL, NULL, false, NULL, NULL, NULL, NULL, NULL);
        //initialize(0, NULL, NULL, NULL,NULL, false, NULL, NULL, NULL, NULL);
    }

    ImageGrabber::ImageGrabber(
        unsigned int cameraNumber,
        std::shared_ptr<Lockable<Camera>> cameraPtr,
        std::shared_ptr<LockableQueue<StampedImage>> newImageQueuePtr,
        QPointer<QThreadPool> threadPoolPtr,
        bool testConfigEnabled,
        string trial_info,
        std::shared_ptr<TestConfig> testConfig,
        std::shared_ptr<Lockable<GetTime>> gettime,
        std::shared_ptr<Lockable<NIDAQUtils>> nidaq_task,
        CmdLineParams& cmdlineparams,
        unsigned int* numImagegrabStarted, 
        QObject *parent
    ) : QObject(parent)
    {
        //cmdLine params
        input_video_dir = cmdlineparams.output_dir;
        isVideo = cmdlineparams.isVideo;
        isSkip = cmdlineparams.isSkip;
        isDebug = cmdlineparams.debug;
        //nframes_ = cmdlineparams.numframes;
        frameGrabAvgTime = cmdlineparams.frameGrabAvgTime; // uint in us: time taken for frame
                                                           //to reach from camera to system buffers
        framerate = cmdlineparams.framerate;
        skip_latency = cmdlineparams.skip_latency;
        ts_match_thres = cmdlineparams.ts_match_thres;

        nframes_ = DEFAULT_TIMING_BUFFER_SIZE;

        std::cout << "Imagegrab Debug :" << isDebug << "Numframes: " << nframes_ << std::endl;
            
        initialize(cameraNumber, cameraPtr, newImageQueuePtr,
            threadPoolPtr,testConfigEnabled, trial_info, testConfig, gettime, nidaq_task, numImagegrabStarted);

    }

    void ImageGrabber::initialize(
        unsigned int cameraNumber,
        std::shared_ptr<Lockable<Camera>> cameraPtr,
        std::shared_ptr<LockableQueue<StampedImage>> newImageQueuePtr,
        QPointer<QThreadPool> threadPoolPtr,
        bool testConfigEnabled,
        string trial_info,
        std::shared_ptr<TestConfig> testConfig,
        std::shared_ptr<Lockable<GetTime>> gettime,
        std::shared_ptr<Lockable<NIDAQUtils>> nidaq_task,
        unsigned int* numImagegrabStarted
    )
    {
        capturing_ = false;
        stopped_ = true;
        cameraPtr_ = cameraPtr;
        threadPoolPtr_ = threadPoolPtr;
        newImageQueuePtr_ = newImageQueuePtr;
        numStartUpSkip_ = DEFAULT_NUM_STARTUP_SKIP;
        cameraNumber_ = cameraNumber;
        testConfigEnabled_ = testConfigEnabled;
        testConfig_ = testConfig;
        trial_num = trial_info;
        fstfrmtStampRef_ = 0.0;
        numImagegrabStarted_ = numImagegrabStarted;
        std::cout << "image grab---" << *numImagegrabStarted << " " << cameraNumber_ << std::endl;

        QPointer<CameraWindow> cameraWindowPtr = getCameraWindow();
        cameraWindowPtrList_ = cameraWindowPtr->getCameraWindowPtrList();
        partnerCameraWindowPtr = getPartnerCameraWindowPtr();

        if ((cameraPtr_ != NULL) && (newImageQueuePtr_ != NULL))
        {
            ready_ = true;
        }
        else
        {
            ready_ = false;
        }
        errorCountEnabled_ = true;

        nidaq_task_ = nidaq_task;
        // needs to be allocated here outside the testConfig.Intend to record nidaq
        // camera trigger timestamps for other threads even if imagegrab is 
        // turned off in testConfig suite
        if (nidaq_task_ != nullptr) {
            nidaq_task_->cam_trigger.resize(nframes_ + DEFAULT_NUM_STARTUP_SKIP, 0);
        }

        gettime_ = gettime;

        if (isVideo) {
            std::cout << "Initializing video" << std::endl;
            initializeVid();
        }

//#if DEBUG
        if (isDebug) {
            process_frame_time_ = 1;
            if (testConfigEnabled_ && !testConfig_->imagegrab_prefix.empty()) {

                if (!testConfig_->f2f_prefix.empty()) {

                    ts_pc.resize(testConfig_->numFrames);
                }

                if (!testConfig_->nidaq_prefix.empty()) {

                    ts_nidaq.resize(testConfig_->numFrames, std::vector<uInt32>(2, 0));
                    ts_nidaqThres.resize(testConfig_->numFrames);
                    imageTimeStamp.resize(testConfig_->numFrames);
                    cam_ts.resize(testConfig_->numFrames);
                    camFrameId.resize(testConfig_->numFrames);
                   
                }

                if (!testConfig_->queue_prefix.empty()) {

                    queue_size.resize(testConfig_->numFrames);
                }

                if (process_frame_time_)
                {
                    ts_process.resize(testConfig_->numFrames, 0);
                    ts_pc.resize(testConfig_->numFrames, 0);
                    queue_size.resize(testConfig_->numFrames, 0);
                    ts_end.resize(testConfig_->numFrames, 0);
                }
            }
        }

//#endif
    }

    void ImageGrabber::initializeVidBackend()
    {
        QString filename;
        if (cameraNumber_ == 0)
        {
            filename = QString::fromStdString(input_video_dir) + "movie_frt.avi";
            std::cout << " movie filename*** " << filename.toStdString() << std::endl;
        }
        else if (cameraNumber_ == 1) {
            filename = QString::fromStdString(input_video_dir) + "movie_sde.avi";
            std::cout << " movie filename **" << filename.toStdString() << std::endl;
        }

        vid_obj_ = new videoBackend(filename);
        cap_obj_ = vid_obj_->videoCapObject();
        vid_numFrames = vid_obj_->getNumFrames(cap_obj_);
        std::cout << "Vid frames: " << vid_numFrames << std::endl;

        if (cap_obj_.isOpened())
            isOpen_ = 1;

    }

    void ImageGrabber::stop()
    {
        stopped_ = true;
    }


    void ImageGrabber::enableErrorCount()
    {
        errorCountEnabled_ = true;
    }

    void ImageGrabber::disableErrorCount()
    {
        errorCountEnabled_ = false;
    }

    void ImageGrabber::run()
    {

        //bool isFirst = true;
        //bool istriggered = false;
        //unsigned long frameCount = 0;
        //unsigned long startUpCount = 0;
        //double dtEstimate = 0.0;

        isFirst = true;
        nidaqTriggered = false;
        frameCount = 0;
        startUpCount = 0;
        dtEstimate = 0.0;
        imagegrab_started = true;
        startTrigger = false;
        stopTrigger = false;
        isFirstTrial = true;
        isDoneWriting = false;
        
        bool done = false;
        bool error = false;
        bool errorEmitted = false;
        unsigned int errorId = 0;
        unsigned int errorCount = 0;
        bool errorMatch = false;
        bool errorMatchFrame = false;
        

        TriggerType trig;

        timeStamp = {0,0};
        timeStampInit = {0,0};

        timeStampDbl = 0.0;
        timeStampDblLast = 0.0;

        cameraFrameCountInit = 0;

        uInt32 read_start = 0, read_end = 0, frameCaptureTime_nidaq = 0;

        uint64_t pc_time=0, start_process=0, end_process=0, time_now=0;
        int64_t start_read_delay=0, end_read_delay = 0;
        int64_t start_push_delay = 0, end_push_delay = 0;
        uint64_t start_delay, end_delay = 0;
        uint64_t expTime = 0, curTime = 0, prev_curTime = 0;
        uint64_t curTime_vid = 0, expTime_vid = 0, delta_now=0;
        uint64_t frameCaptureTime, avg_frameLatSinceFirstFrame = 0;
        int64_t  wait_thres, avgwait_time, delay_framethres;
        uint64_t fast_clock_period;
        StampedImage stampImg;
        int numtrailFrames = 0;
        
        uInt32 nidaq_ts_curr=0, nidaq_ts_init = 0;

        QString errorMsg("no message");

        if (isVideo) {
            frameCaptureTime = static_cast<uint64_t>((1.0 / (float)framerate) * 1000000);
            frameCaptureTime_nidaq = 125;
            wait_thres = static_cast<int64_t>(100);
            avgwait_time = 0;
            int num_skipFrames = 0;
        }
        else {
 
            frameCaptureTime = static_cast<uint64>((1.0/(float)framerate) * 1000000);  // unit uses
            wait_thres = static_cast<int64>(skip_latency - frameGrabAvgTime);
            avgwait_time = 0;
            if (nidaq_task_ != nullptr)
            {
                fast_clock_period = static_cast<uint64>(1.0/ float(nidaq_task_->fast_counter_rate) * 1000000); // uint usecs
            }
            std::cout << "Wait thres skip " << wait_thres << "frameCaptureTime " << frameCaptureTime 
                       <<  "frameGrabTime " << frameGrabAvgTime 
                       << "nidaq ts match thres " << ts_match_thres 
                       << "nidaq fast clock period " << fast_clock_period  
                       << std::endl;
        }

        if (!ready_)
        {
            return;
        }

        // Set thread priority to "time critical" and assign cpu affinity
        QThread *thisThread = QThread::currentThread();
        thisThread->setPriority(QThread::TimeCriticalPriority);
        //ThreadAffinityService::assignThreadAffinity(true, cameraNumber_);

        trig = cameraPtr_->getTriggerType();

        //set nidaq trigger for partner camera
        partnerImageGrabberPtr = partnerCameraWindowPtr->getImageGrabberPtr();
        QPointer<CameraWindow>cameraWindowPtr = getCameraWindow();

        connect(
            this,
            SIGNAL(nidaqtriggered(bool)),
            partnerImageGrabberPtr,
            SLOT(setTriggered(bool))
        );
        connect(
            this,
            SIGNAL(setImagegrabParams()),
            partnerImageGrabberPtr,
            SLOT(resetParams())

        );

        // Start image capture
        cameraPtr_->acquireLock();
        try
        {
            cameraPtr_->startCapture();
            if (nidaq_task_ != nullptr) {

                cameraPtr_->setupNIDAQ(nidaq_task_, testConfigEnabled_,
                    trial_num, testConfig_,
                    gettime_, cameraNumber_);

            }
            else {
                printf("nidaq not set");
            }
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_->releaseLock();

        if (error)
        {
            emit startCaptureError(errorId, errorMsg);
            errorEmitted = true;
            return;

        }

        acquireLock();
        stopped_ = false;
        releaseLock();

        //// TEMPORARY - for mouse grab detector testing
        //// ------------------------------------------------------------------------------

        //// Check for existence of movie file
        //QString grabTestMovieFileName("bias_test.avi");
        //cv::VideoCapture fileCapture;
        //unsigned int numFrames = 0;
        //int fourcc = 0;
        //bool haveGrabTestMovie = false;

        //if (QFileInfo(grabTestMovieFileName).exists())
        //{
        //    fileCapture.open(grabTestMovieFileName.toStdString());
        //    if ( fileCapture.isOpened() )
        //    {
        //        numFrames = (unsigned int)(fileCapture.get(CV_CAP_PROP_FRAME_COUNT));
        //        fourcc = int(fileCapture.get(CV_CAP_PROP_FOURCC));
        //        haveGrabTestMovie = true;
        //    }
        //}
        //// -------------------------------------------------------------------------------
        std::cout << "Imagegrab started " << std::endl;

        acquireLock();
        std::cout << "image grab--value " << *numImagegrabStarted_
            << " " << cameraNumber_ << " " << imagegrab_started << std::endl;
        (*numImagegrabStarted_)--;
        std::cout << "image grab---run" << *numImagegrabStarted_
            << " " << cameraNumber_ << std::endl;

        if (*numImagegrabStarted_ != 0) {
            std::cout << "image grab---wait" << *numImagegrabStarted_
                << " " << cameraNumber_ << std::endl;
            waitForCond();  
        }
        else if (*numImagegrabStarted_ == 0) {
            std::cout << "image grab---wake all " << *numImagegrabStarted_
                << " " << cameraNumber_ << std::endl;
            signalCondMet();
            partnerImageGrabberPtr->signalCondMet();
            startTrigger = true;
            partnerImageGrabberPtr->startTrigger = true;
        }
        releaseLock();

        // Grab images from camera until the done signal is given
        while (!done)
        {

            acquireLock();
            done = stopped_;
            releaseLock();

            // sync nidaq trigger start/stop without thread restart
            if (startTrigger)
            {

                resetNidaqTrigger(true);
                startTrigger = false;

            }else if(stopTrigger)
            {
                resetNidaqTrigger(false);
                stopTrigger = false;
                nidaqTriggered = false;
                frameCount = 0;
                if(!isVideo)
                    flushCameraBuffer();
                //cameraWindowPtr->stopTrigger();
                emit(signalGpuDeinit());
                std::cout << "CameraNumber " << cameraNumber_ << "Stop Trigger " << stopTrigger << std::endl;

            }else{}

            // in debug mode done writing
            if (isDebug)
            {
                if (isDoneWriting)
                {
                    QThread::yieldCurrentThread();
                    continue;
                }
            }

            start_process = gettime_->getPCtime();

            if (isVideo) {
                delay_framethres = 0;

                if (frameCount == vid_numFrames) {
                    //stopTrigger = true;
                    //frameCount = 0;
                    expTime = 0, curTime = 0, prev_curTime = 0;
                    curTime_vid = 0, expTime_vid = 0, delta_now = 0;
                    avgwait_time = 0;
                    QThread::yieldCurrentThread();
                    continue;
                }

                if (nidaq_task_->istrig) {
                    if (nidaq_task_ != nullptr && frameCount == 0) {
                        
                        //reset some timing params
                        expTime = 0, curTime = 0, prev_curTime = 0;
                        curTime_vid = 0, expTime_vid = 0, delta_now = 0;
                        avgwait_time = 0;

                        //get first frame ts
                        fstfrmtStampRef_ = static_cast<uint64_t>(start_process);
                        prev_curTime = expTime_vid;
                    }

                    expTime_vid = fstfrmtStampRef_ + (frameCaptureTime * (frameCount + 1));
                    if (frameCount == 0) {
                        prev_curTime = expTime_vid;
                    }

                    if (startUpCount >= numStartUpSkip_) {

                        // adding synthetic latency to the frame grab 
                        if (!delayFrames.empty() && frameCount == delayFrames.top())
                        {

                            // get magnitude of latency to generate - sampled randomly
                            if (!latency_spikes.empty())
                            {
                                delay_framethres = latency_spikes.back();
                                delay_view[frameCount][1] = delay_framethres;
                                delay_view[frameCount][0] = 1;
                                latency_spikes.pop_back();

                            }

                            // introduce the latency delay
                            start_delay = gettime_->getPCtime();
                            end_delay = start_delay;
                            while ((end_delay - start_delay) < delay_framethres)
                            {
                                end_delay = gettime_->getPCtime();
                            }

                            delayFrames.pop();

                        }
                        else if ((prev_curTime - expTime_vid) > 0 &&
                            static_cast<int64_t>(prev_curTime - expTime_vid) > wait_thres) {

                            delay_view[frameCount][0] = 1;
                            delay_view[frameCount][1] = static_cast<int64_t>(prev_curTime - expTime_vid);
                        }
                        else {
                            //std::cout << "Get Image" << std::endl;
                            //nidaq_task_->getCamtrig(frameCount);

                        }

                    }

                    // wait for nidaq trigger signal to grab frame
                    stampImg.image = vid_images[frameCount].image;

                }
                else {
                    //std::cout << "Yield thread Video" << std::endl;
                    QThread::yieldCurrentThread();
                    continue;
                }

            }
            else {
          
                // if number of frames to capture have been reached 
                // set stop trigger and frameCount reset
                /*if (frameCount == nframes_) {
                    stopTrigger = true;
                    frameCount = 0;
                    QThread::yieldCurrentThread();
                    continue;
                }*/

                // if nidaq is not triggered yet do not capture frames
                if (frameCount == 0 && !nidaq_task_->istrig)
                {
                    QThread::yieldCurrentThread();
                    continue;
                }

                   
                //grab images from camera 
                //start_process = gettime_->getPCtime();
                cameraPtr_->acquireLock();
                try
                {

                    stampImg.image = cameraPtr_->grabImage();
                    stampImg.isSpike = false;
                    timeStamp = cameraPtr_->getImageTimeStamp();
                    cam_frameId = cameraPtr_->getFrameId();
                    error = false;
                }
                catch (RuntimeError &runtimeError)
                {
                    std::cout << "Frame grab error: id = ";
                    std::cout << runtimeError.id() << ", what = ";
                    std::cout << runtimeError.what() << std::endl;
                    error = true;
                }
                cameraPtr_->releaseLock();
                //end_process = gettime_->getPCtime();

            }
            
            // grabImage is nonblocking - returned frame is empty is a new frame is not available.
            if (stampImg.image.empty())
            {
                if (isDebug) {
                    //std:cout << "Empty images" << std::endl;
                }
                QThread::yieldCurrentThread();
                continue;
            }

            // Push image into new image queue
            if (!error)
            {
                errorCount = 0;                  // Reset error count 
                timeStampDblLast = timeStampDbl; // Save last timestamp

                // Set initial time stamp for fps estimate
                if ((startUpCount == 0) && (numStartUpSkip_ > 0))
                {
                    timeStampInit = timeStamp;
                    cameraFrameCountInit = cam_frameId;

                }
                timeStampDbl = convertTimeStampToDouble(timeStamp, timeStampInit);
                cameraFrameCount = convertCameraFrameCount(cam_frameId, cameraFrameCountInit);

                //set the initial nidaq ts
                if (nidaq_task_ != nullptr && startUpCount >= numStartUpSkip_
                    && nidaq_task_->istrig) {

                    //get nidaq trigger timestamp
                    nidaq_task_->getCamtrig(frameCount, nframes_);
                    if (frameCount == 0)
                    {
                        nidaq_ts_init = nidaq_task_->cam_trigger[frameCount%nframes_];
                    }
                    nidaq_ts_curr = nidaq_task_->cam_trigger[frameCount%nframes_];

                    //reset nidaq buffer for future frames
                    //reset is important as there is a single read from nidaq per frame
                    //and we do not use the same values from previouds frames
                    nidaq_task_->cam_trigger[(frameCount + ((nframes_) / 2)) % nframes_] = 0;
                }
                else {
                    if (isDebug) {
                        //std::cout << "DEBUG:: Nidaq is stopped " << cameraNumber_ << std::endl;
                    }
                }

                if (isFirstTrial) {
                    // Skip some number of frames on startup - recommened by Point Grey. 
                    // During this time compute running avg to get estimate of frame interval
                    if (startUpCount < numStartUpSkip_)
                    {
                        double dt = timeStampDbl - timeStampDblLast;
                        if (startUpCount == MIN_STARTUP_SKIP)
                        {
                            dtEstimate = dt;
                        }
                        else if (startUpCount > MIN_STARTUP_SKIP)
                        {
                            double c0 = double(startUpCount - 1) / double(startUpCount);
                            double c1 = double(1.0) / double(startUpCount);
                            dtEstimate = c0 * dtEstimate + c1 * dt;
                        }

                        if (nidaq_task_ != nullptr && startUpCount < numStartUpSkip_ && nidaq_task_->istrig) {

                            nidaq_task_->acquireLock();

                            if (nidaq_task_->cam_trigger[nframes_ + startUpCount] == 0) {
                                DAQmxErrChk(DAQmxReadCounterScalarU32(nidaq_task_->taskHandle_trigger_in, 10.0, &read_buffer_, NULL));
                                nidaq_task_->cam_trigger[nframes_ + startUpCount] = read_buffer_;
                            }
                            nidaq_task_->releaseLock();

                        }

                        startUpCount++;
                        if (startUpCount == numStartUpSkip_) {
                            isFirstTrial = false;
                        }
                        continue;
                    }
                }

                //std::cout << "dt grabber: " << timeStampDbl - timeStampDblLast << std::endl;

                // Reset initial time stamp for image acquisition
                if ((isFirst) && (startUpCount >= numStartUpSkip_))
                {
                    cameraFrameCountInit = cam_frameId;
                    timeStampInit = timeStamp;
                    timeStampDblLast = 0.0;
                    isFirst = false;
                    timeStampDbl = convertTimeStampToDouble(timeStamp, timeStampInit);
                    cameraFrameCount = convertCameraFrameCount(cam_frameId, cameraFrameCountInit);
                    emit startTimer();
                }

                //// TEMPORARY - for mouse grab detector testing
                //// --------------------------------------------------------------------- 
                //cv::Mat fileMat;
                //StampedImage fileImg;
                //if (haveGrabTestMovie)
                //{
                //    fileCapture >> fileMat; 
                //    if (fileMat.empty())
                //    {
                //        fileCapture.set(CV_CAP_PROP_POS_FRAMES,0);
                //        continue;
                //    }

                //    cv::Mat  fileMatMono = cv::Mat(fileMat.size(), CV_8UC1);
                //    cvtColor(fileMat, fileMatMono, CV_RGB2GRAY);
                //    
                //    cv::Mat camSizeImage = cv::Mat(stampImg.image.size(), CV_8UC1);
                //    int padx = camSizeImage.rows - fileMatMono.rows;
                //    int pady = camSizeImage.cols - fileMatMono.cols;

                //    cv::Scalar padColor = cv::Scalar(0);
                //    cv::copyMakeBorder(fileMatMono, camSizeImage, 0, pady, 0, padx, cv::BORDER_CONSTANT, cv::Scalar(0));
                //    stampImg.image = camSizeImage;
                //}

                // ---------------------------------------------------------------------
                // Test Configuration
                //------------------------------------------------------------------------
                if (!isVideo) {
                    
                    if (nidaq_task_ != nullptr && nidaq_task_->istrig){
                        if (frameCount == 0) {
                            
                            fstfrmtStampRef_ = static_cast<uint64_t>(nidaq_task_->cam_trigger[frameCount%nframes_]);
                            
                        }
                        nidaq_task_->getNidaqTimeNow(read_ondemand_);

                    }
                    else {
                        //fstfrmtStampRef_ = static_cast<uint64_t>(gettime_->getPCtime());
                    }
                }

                //match frameCount from camera and current frameCount from BIAS
                if (!isVideo) {
                    errorMatchFrame = matchCameraFrameCount(cameraFrameCount, frameCount);
                    if (!errorMatchFrame) {

                        errorMsg = QString::fromStdString("Camera framecount does not match BIAS framecount ");
                        errorMsg += QString::number(frameCount);
                        emit framecountMatchError(0, errorMsg);

                    }
                }

                // match nidaq ts to camera timestamp
                /*if (!isVideo) {
                    errorMatch = matchNidaqToCameraTimeStamp(nidaq_ts_curr, nidaq_ts_init, timeStampDbl, frameCount);
                    if (!errorMatch) {

                        errorMsg = QString::fromStdString("Nidaq ts does not match camera ts ");
                        errorMsg += QString::number(frameCount);
                        emit nidaqtsMatchError(0, errorMsg);

                    }
                }*/

                if (isVideo) {
                    //expTime_vid = fstfrmtStampRef_ + (frameCaptureTime * (frameCount+1));
                    curTime_vid = max(fstfrmtStampRef_ + (frameCaptureTime*(frameCount + 1) + delay_framethres), prev_curTime);
                    prev_curTime = curTime_vid;
                    avgwait_time = curTime_vid - expTime_vid;
                    //std::cout << "Avg wait time" << avgwait_time << std::endl;
                } 
                else {
                    avg_frameLatSinceFirstFrame = (frameCaptureTime * frameCount) + frameGrabAvgTime; // time in us
                    expTime = (static_cast<uint64_t>(fstfrmtStampRef_) * fast_clock_period) + avg_frameLatSinceFirstFrame; //time in us
                    curTime = (static_cast<uint64_t>(read_ondemand_) * fast_clock_period); //time in us
                    avgwait_time = curTime - expTime;
                }
                
      
                /*if (isSkip) {
                    // Set image data timestamp, framecount and frame interval estimate
                    if (abs(avgwait_time) <= wait_thres)              
                    {
                        stampImg.timeStamp = timeStampDbl;
                        stampImg.timeStampInit = timeStampInit;
                        stampImg.timeStampVal = timeStamp;
                        stampImg.frameCount = frameCount;
                        stampImg.dtEstimate = dtEstimate;
                        stampImg.fstfrmtStampRef = fstfrmtStampRef_;

                        newImageQueuePtr_->acquireLock();
                        newImageQueuePtr_->push(stampImg);
                        newImageQueuePtr_->signalNotEmpty();
                        newImageQueuePtr_->releaseLock();
                        //std::cout << "FrameCount" << frameCount << std::endl;
                    }
                    else {
                      
                        if (isDebug && testConfigEnabled_ && nidaq_task_ != nullptr)
                            ts_nidaqThres[frameCount] = 1.0;
                        std::cout << "skipped in imagegrab " << frameCount 
                            << " cameraNumber " << cameraNumber_ 
                            << " Avg wait time " << abs(avgwait_time) << std::endl;
                    }
                }
                else {
                    stampImg.timeStamp = timeStampDbl;
                    stampImg.timeStampInit = timeStampInit;
                    stampImg.timeStampVal = timeStamp;
                    stampImg.frameCount = frameCount;
                    stampImg.dtEstimate = dtEstimate;
                    stampImg.fstfrmtStampRef = fstfrmtStampRef_;

                    newImageQueuePtr_->acquireLock();
                    newImageQueuePtr_->push(stampImg);
                    newImageQueuePtr_->signalNotEmpty();
                    newImageQueuePtr_->releaseLock();
                    //std::cout << "Not skipped " << frameCount << std::endl;
                }*/

                if (isSkip) {
                    if (abs(avgwait_time) <= wait_thres)
                    {
                        stampImg.isSpike = false;
                    }
                    else {
                        stampImg.isSpike = true;
                        
                        if (isDebug && testConfigEnabled_ && nidaq_task_ != nullptr)
                            ts_nidaqThres[frameCount] = 1.0;

                        //std::cout << "skipped to plugin " << frameCount
                        //    << " cameraNumber " << cameraNumber_
                        //    << " Avg wait time " << abs(avgwait_time) << std::endl;
                    }
                }
                else {
                    stampImg.isSpike = false;
                }
                stampImg.timeStamp = timeStampDbl;
                stampImg.timeStampInit = timeStampInit;
                stampImg.timeStampVal = timeStamp;
                stampImg.frameCount = frameCount;
                stampImg.dtEstimate = dtEstimate;
                stampImg.fstfrmtStampRef = fstfrmtStampRef_;

                newImageQueuePtr_->acquireLock();
                newImageQueuePtr_->push(stampImg);
                newImageQueuePtr_->signalNotEmpty();
                newImageQueuePtr_->releaseLock();
                end_process = gettime_->getPCtime();
                frameCount++;
                
                if (isDebug) {
                    if (testConfigEnabled_ && ((frameCount - 1) < testConfig_->numFrames)) {

                        if (nidaq_task_ != nullptr) {

                            if (!testConfig_->imagegrab_prefix.empty() && !testConfig_->nidaq_prefix.empty())
                            {

                                nidaq_task_->acquireLock();
                                ts_nidaq[frameCount - 1][0] = nidaq_task_->cam_trigger[(frameCount - 1)%nframes_];
                                nidaq_task_->releaseLock();
                                ts_nidaq[frameCount - 1][1] = read_ondemand_;
                                imageTimeStamp[frameCount - 1] = timeStampDbl;
                                cam_ts[frameCount - 1] = timeStamp.seconds*UINT64_C(1000000) 
                                                         + static_cast<uint64_t>(timeStamp.microSeconds);
                                camFrameId[frameCount - 1] = cam_frameId;

                            }      
                        }
                    }
                }

                ///---------------------------------------------------------------
//#if DEBUG
                if (isDebug) {
                 
                    if (testConfigEnabled_ && ((frameCount - 1) < testConfig_->numFrames)) {

                        if (!testConfig_->imagegrab_prefix.empty()) {

                            if (!testConfig_->f2f_prefix.empty()) {

                                pc_time = gettime_->getPCtime();
                                if (frameCount <= unsigned long(testConfig_->numFrames))
                                    ts_pc[frameCount - 1] = pc_time;
                            }

                            if (!testConfig_->queue_prefix.empty()) {

                                if (frameCount <= unsigned long(testConfig_->numFrames))
                                    queue_size[frameCount - 1] = newImageQueuePtr_->size();

                            }

                            if (process_frame_time_)
                            {
                                if (frameCount <= unsigned long(testConfig_->numFrames)) {
                                    ts_process[frameCount - 1] = end_process - start_process;
                                    ts_pc[frameCount - 1] = start_process;
                                    ts_end[frameCount - 1] = end_process;
                                }
                            }
                        }

                        if (frameCount == testConfig_->numFrames)
                        {
                            if(!testConfig_->f2f_prefix.empty())
                            {

                                std::string filename = testConfig_->dir_list[0] + "/"
                                + testConfig_->f2f_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + testConfig_->f2f_prefix + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                gettime_->write_time_1d<int64_t>(filename, testConfig_->numFrames, ts_pc);

                            }

                    
                            if(!testConfig_->nidaq_prefix.empty())
                            {
                                
                                std::string filename = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + testConfig_->nidaq_prefix + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                std::string filename1 = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + testConfig_->nidaq_prefix + "_thres" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";


                                std::string filename2 = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + "imagetimestamp_" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                std::string filename3 = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + "camts_" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                std::string filename4 = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + "camframeId_" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                gettime_->write_time_2d<uInt32>(filename, testConfig_->numFrames, ts_nidaq);
                                gettime_->write_time_1d<float>(filename1, testConfig_->numFrames, ts_nidaqThres);
                                gettime_->write_time_1d<double>(filename2, testConfig_->numFrames, imageTimeStamp);
                                gettime_->write_time_1d<uint64_t>(filename3, testConfig_->numFrames, cam_ts);
                                gettime_->write_time_1d<int64_t>(filename4, testConfig_->numFrames, camFrameId);
                            }

                            if(!testConfig_->queue_prefix.empty()) {

                                string filename = testConfig_->dir_list[0] + "/"
                                + testConfig_->queue_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + testConfig_->queue_prefix + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                gettime_->write_time_1d<unsigned int>(filename, testConfig_->numFrames, queue_size);

                            }

                            if(process_frame_time_) {

                                string filename = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + "process_time" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                string filename1 = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + "start_time" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";


                                string filename2 = testConfig_->dir_list[0] + "/"
                                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                + testConfig_->imagegrab_prefix
                                + "_" + "end_time" + "cam"
                                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                //std::cout << "Writing" << std::endl;
                                gettime_->write_time_1d<int64_t>(filename, testConfig_->numFrames, ts_process);
                                gettime_->write_time_1d<int64_t>(filename1, testConfig_->numFrames, ts_pc);
                                gettime_->write_time_1d<int64_t>(filename2, testConfig_->numFrames, ts_end);
                                //std::cout << "Written" << std::endl;
                            }

                            if (isVideo) {
   
                                string filename = testConfig_->dir_list[0] + "/"
                                    + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                                    + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                                    + testConfig_->imagegrab_prefix
                                    + "_" + "skipped_frames" + "cam"
                                    + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";

                                gettime_->write_time_2d<int64_t>(filename, testConfig_->numFrames, delay_view);
                               
                            }

                            isDoneWriting = true;
                        }
                    }
                }
//#endif
            }
            else {

                if (errorCountEnabled_)
                {
                    errorCount++;
                    if (errorCount > MAX_ERROR_COUNT)
                    {
                        errorId = ERROR_CAPTURE_MAX_ERROR_COUNT;
                        errorMsg = QString("Maximum allowed capture error count reached");
                        if (!errorEmitted)
                        {
                            emit captureError(errorId, errorMsg);
                            errorEmitted = true;
                        }
                    }
                }
            }

        } // while (!done)

        // Stop image capture
        std::cout << "Imagegrabber exited " << std::endl;
        error = false;
        cameraPtr_->acquireLock();
        try
        {
            cameraPtr_->stopCapture();
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_->releaseLock();

        if ((error) && (!errorEmitted))
        {
            emit stopCaptureError(errorId, errorMsg);
        }
        //#endif
    }

    /*void ImageGrabber::run()
    {
        unsigned long frameCount = 0;
        cv::Mat img;

        uint64_t start_read_delay = 0, end_read_delay = 0;
        unsigned int avgwaitThres_us = 2500;

        bool error = false;
        bool errorEmitted = false;
        unsigned int errorId = 0;
        unsigned int errorCount = 0;
        QString errorMsg;

        if (!ready_)
        {
            return;
        }

        // Set thread priority to "time critical" and assign cpu affinity
        QThread *thisThread = QThread::currentThread();
        thisThread->setPriority(QThread::TimeCriticalPriority);
        //ThreadAffinityService::assignThreadAffinity(true, cameraNumber_);

        initializeVid();

        while (frameCount < testConfig_->numFrames)
        {
            start_read_delay = gettime_->getPCtime();
            end_read_delay = start_read_delay;
            while ((end_read_delay - start_read_delay) < avgwaitThres_us)
            {
                end_read_delay = gettime_->getPCtime();
            }
            img = vid_images[frameCount].image;
            end_read_delay = gettime_->getPCtime();
            ts_process[frameCount] = end_read_delay - start_read_delay;
            frameCount++;
        }

        if (frameCount == testConfig_->numFrames)
        {
            string filename = testConfig_->dir_list[0] + "/"
                + testConfig_->nidaq_prefix + "/" + testConfig_->cam_dir
                + "/" + testConfig_->git_commit + "_" + testConfig_->date + "/"
                + testConfig_->imagegrab_prefix
                + "_" + "process_time" + "cam"
                + std::to_string(cameraNumber_) + "_" + trial_num + ".csv";
            gettime_->write_time_1d<int64_t>(filename, testConfig_->numFrames, ts_process);
            std::cout << "Written" << std::endl;

        }

    }*/

    double ImageGrabber::convertTimeStampToDouble(TimeStamp curr, TimeStamp init)
    {
        double timeStampDbl = 0;
        timeStampDbl = double(curr.seconds);
        timeStampDbl -= double(init.seconds);
        timeStampDbl += (1.0e-6)*double(curr.microSeconds);
        timeStampDbl -= (1.0e-6)*double(init.microSeconds);
        return timeStampDbl;
    }

    unsigned long ImageGrabber::convertCameraFrameCount(int64_t camera, int64_t cameraInit)
    {
        unsigned long frameCountConvert = 0;
        frameCountConvert = camera - cameraInit;
        return frameCountConvert;
    }

    bool ImageGrabber::matchNidaqToCameraTimeStamp(uInt32& nidaqts_curr, uInt32& nidaqts_init, 
                                                   double& camts_curr, uint64_t frameCount)
    {
        double nidaqDbl = 0.0;
        double ts_diff = 0.0;
        nidaqDbl = static_cast<double>((nidaqts_curr - nidaqts_init) * (1/nidaq_task_->fast_counter_rate));
        ts_diff = abs(nidaqDbl - camts_curr); // difference in seconds
        
        //std::cout << "Nidaq match camera ts "  << 
        //    nidaqDbl <<  " "  << camts_curr << " "  << ts_diff << " " << frameCount << std::endl;

        if (ts_diff > (ts_match_thres)) // threshold in seconds 
        {
            return false;

        }else{

            return true;

        }

    }

    bool ImageGrabber::matchCameraFrameCount(unsigned long cameraframeCount_curr,
        unsigned long frameCount_curr)
    {
        unsigned long frame_diff = 0;
        frame_diff = cameraframeCount_curr - frameCount_curr;

        if (frame_diff > 0)
        {
            return false;
        }
        else {
            return true;
        }
        
    }

    /*void ImageGrabber::spikeDetected(unsigned int frameCount) {

        float imgGrab_time = (ts_nidaq[frameCount][1] - ts_nidaq[frameCount][0]) * 0.02;
        if (imgGrab_time > testConfig_->latency_threshold)
        {
            //cameraPtr_->skipDetected(stampImg);
            stampImg.isSpike = true;
            assert(stampImg.frameCount == frameCount);
            ts_nidaqThres[frameCount] = imgGrab_time;
        }

    }*/

    unsigned int ImageGrabber::getPartnerCameraNumber()
    {
        // Returns camera number of partner camera. For this example
        // we just use camera 0 and 1. In another setting you might do
        // this by GUID or something else.
        if (cameraNumber_ == 0) {
            return 1;
        } else {
            return 0;
        }
    }

    QPointer<CameraWindow> ImageGrabber::getCameraWindow()
    {
        QPointer<CameraWindow> cameraWindowPtr = (CameraWindow*)(parent());
        return cameraWindowPtr;
    }

    QPointer<CameraWindow> ImageGrabber::getPartnerCameraWindowPtr()
    {
        QPointer<CameraWindow> partnerCameraWindowPtr = nullptr;
        if ((cameraWindowPtrList_->size()) > 1)
        {
            for (auto cameraWindowPtr : *cameraWindowPtrList_)
            {
                partnerCameraNumber_ = getPartnerCameraNumber();

                if ((cameraWindowPtr->getCameraNumber()) == partnerCameraNumber_)
                {

                    partnerCameraWindowPtr = cameraWindowPtr;
                }
            }
        }
        return partnerCameraWindowPtr;
    }

    void ImageGrabber::initiateVidSkips(priority_queue<int, vector<int>,
        greater<int>>& skip_frames)
    {
        bool isreg = 0;
        unsigned int spike_mag=0;
        unsigned int min_latency_spike=0, max_latency_spike = 20000;
        srand(time(NULL));

        int framenumber=0;
        set<int> s;

        // create random selected frames for skipping 
        while(s.size() != no_of_skips)
        {
            if (isreg) {

                framenumber += 100;
                s.insert(framenumber);
            }
            else
            {
              
                framenumber = rand() % (vid_numFrames-1);
                s.insert(framenumber);
            }
    
        }
        
        set<int>::iterator it;
        if (!s.empty())
        {
            for (it = s.begin(); it != s.end(); it++)
            {
                skip_frames.push(*it);

                // create random latency for skips in us
                spike_mag = (rand() % (max_latency_spike - min_latency_spike))
                    + min_latency_spike;
                latency_spikes.push_back(spike_mag);

            }
        }

    }

    void ImageGrabber::readVidFrames()
    {
        StampedImage stampedImg;
        vid_images.resize(vid_numFrames);
        int count = 0;

        while (isOpen_ && (count < vid_numFrames))
        {

            stampedImg.image = vid_obj_->getImage(cap_obj_);
            stampedImg.frameCount = count;
            vid_images[count] = stampedImg;
            count = count + 1;

        }
        vid_obj_->releaseCapObject(cap_obj_);

        QPointer<CameraWindow> cameraWindowPtr = getCameraWindow();
        cameraWindowPtr->vidFinsihed_reading = 1;
        std::cout << "Finished reading " << std::endl;
        /*if (cameraNumber_ == 1)
        {
            emit cameraWindowPtr->finished_vidReading();
        }*/
    }

    void ImageGrabber::initializeVid()
    {
        no_of_skips = 0;

        initializeVidBackend();
        initiateVidSkips(delayFrames);
        delay_view.resize(vid_numFrames, vector<int64_t>(2,0));

        //important this is the last function called before imagegrabber starts 
        //grabbing frames. The camera trigger signal is on and the both threads
        // want to read frames at the same time. 
        readVidFrames();

    }

    void ImageGrabber::setTriggered(bool istriggered)
    {
        acquireLock();
        std::cout << "before nidaq Trigger " << nidaqTriggered << std::endl;
        nidaqTriggered = istriggered;
        std::cout << "after nidaq Trigger " << nidaqTriggered << std::endl;
        releaseLock();
        
    }

    void ImageGrabber::setTrialNum(string trialnum)
    {
        trial_num = trialnum;
    }

    /*void ImageGrabber::resetNidaqTrigger(bool turnOn)
    {
        std::cout << "Nidaq has to be turned " << turnOn << " " << cameraNumber_ <<  std::endl;
        //std::cout << "Nidaq trigger value entering the reset fnc " << nidaqTriggered << cameraNumber_ << std::endl;
        //std::cout << "Nidaq trigger value entering the reset fnc " << partnerImageGrabberPtr->nidaqTriggered << std::endl;
        if (nidaqTriggered != turnOn) {
            acquireLock();
            if (turnOn)
            {
                resetParams();
            }
            nidaqTriggered = turnOn;
            if (partnerImageGrabberPtr->nidaqTriggered != turnOn)
            {
                std::cout << "Nidaq wait " << cameraNumber_ << std::endl;
                waitForCond();
            }
            else if (partnerImageGrabberPtr->nidaqTriggered == turnOn)
            {
                std::cout << "Nidaq wake " << cameraNumber_ << std::endl;
                //signalCondMet();
                if (turnOn && !nidaq_task_->start_tasks && !nidaq_task_->istrig) {
                    //std::cout << "Nidaq is triggered " << cameraNumber_ << std::endl;
                    nidaq_task_->startTasks();
                    nidaq_task_->start_trigger_signal();
                }

                if (!turnOn)
                {
                    nidaq_task_->stopTasks();
                    nidaq_task_->stop_trigger_signal();
                }

                partnerImageGrabberPtr->signalCondMet();
                //std::cout << "Nidaq is turned " << nidaqTriggered << " " << cameraNumber_ << std::endl;
                //std::cout << "Nidaq is turned " << partnerImageGrabberPtr->nidaqTriggered << " " << cameraNumber_ << std::endl;
            }
            releaseLock();
        }
        std::cout << "Nidaq Reset exited " << cameraNumber_ << std::endl;
    }*/

    void ImageGrabber::resetNidaqTrigger(bool turnOn)
    {

        std::cout << "Nidaq has to be turned " << turnOn << " " << cameraNumber_ << std::endl;
        if (turnOn)
        {
            resetParams();
        }

        if (cameraNumber_ == 0)
        {
            if (turnOn && !nidaq_task_->start_tasks && !nidaq_task_->istrig)
            {
                nidaq_task_->startTasks();
                nidaq_task_->start_trigger_signal();
                nidaqTriggered = nidaq_task_->istrig;
                emit nidaqtriggered(nidaqTriggered);
                std::cout << "cameraNumber " << cameraNumber_ << "Exiting wait " 
                    "nidaq flag set " << nidaq_task_->istrig << std::endl;
            }

            if (!turnOn)
            {
                nidaq_task_->stopTasks();
                nidaq_task_->stop_trigger_signal();
                nidaqTriggered = nidaq_task_->istrig;
                emit nidaqtriggered(nidaqTriggered);
            }
        }
        else if (cameraNumber_ != 0) {

            if (turnOn && !nidaq_task_->istrig) {
                
                bool wait = false;
                std::cout << "cameraNumber " << cameraNumber_ << "Entering wait " 
                    << "nidaq flag set " << nidaq_task_->istrig <<  std::endl;
                while (!wait)
                {
                    acquireLock();
                    wait = nidaqTriggered;
                    releaseLock();
                }
                std::cout << "cameraNumber " << cameraNumber_ << "Exiting wait " << std::endl;
            }

            if (!turnOn)
            {
                std::cout << "cameraNumber " << cameraNumber_ << "Entering wait "
                    << "nidaq flag reset " << nidaq_task_->istrig << std::endl;
                bool wait = true;
                while (wait)
                {
                    acquireLock();
                    wait = nidaqTriggered;
                    releaseLock();

                }
                std::cout << "cameraNumber " << cameraNumber_ << "Exiting wait " << std::endl;
            }
            
        }
        std::cout << "Nidaq Reset exited " << cameraNumber_ << std::endl;
    }

    void ImageGrabber::resetParams()
    {
        isFirst = true;
        if (!isFirstTrial)
            startUpCount = DEFAULT_NUM_STARTUP_SKIP;
        else
            startUpCount = 0;
        dtEstimate = 0.0;
        frameCount = 0;

        timeStamp = { 0,0 };
        timeStampInit = { 0,0 };

        timeStampDbl = 0.0;
        timeStampDblLast = 0.0;
        fstfrmtStampRef_ = 0.0;

        isReset = true;
        nidaqTriggered = false;

        if (isDebug)
            isDoneWriting = false;

        std::cout << " Image grab params set for camera number: " << cameraNumber_ << std::endl;
    }

    //this is used to reset the camera buffers before starting next 
    //set of capture of images without stopping the imagegrab thread
    void ImageGrabber::flushCameraBuffer()
    {
        StampedImage stampImg;
        unsigned int frameCount_before=0 ,frameCount_after = 0;

        cameraPtr_->acquireLock();
        try
        {

            stampImg.image = cameraPtr_->grabImage();
            frameCount_before = cameraPtr_->getFrameId();

        }
        catch (RuntimeError &runtimeError)
        {
            std::cout << "Frame grab error: id = ";
            std::cout << runtimeError.id() << ", what = ";
            std::cout << runtimeError.what() << std::endl;
  
        }
        cameraPtr_->releaseLock();

        while (!stampImg.image.empty())
        {
            cameraPtr_->acquireLock();
            try
            {

                stampImg.image = cameraPtr_->grabImage();
                //std::cout << "Frame flushed " << std::endl;
            }
            catch (RuntimeError &runtimeError)
            {
                std::cout << "Frame grab error: id = ";
                std::cout << runtimeError.id() << ", what = ";
                std::cout << runtimeError.what() << std::endl;

            }
            cameraPtr_->releaseLock();

        }

        //get the last frameCount from the camera
        cameraPtr_->acquireLock();
        frameCount_after = cameraPtr_->getFrameId();
        cameraPtr_->releaseLock();
        std::cout << "Number of frames skipped " << (frameCount_after - frameCount_before)+1 << std::endl;

    }
   
} // namespace bias


