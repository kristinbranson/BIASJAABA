#ifndef PROCESS_SCORES_HPP
#define PROCESS_SCORES_HPP

#include "lockable.hpp"
#include "stamped_image.hpp"
#include "frame_data.hpp"
#include "HOGHOF.hpp"
#include "beh_class.hpp"
#include <opencv2/opencv.hpp>
#include <QThreadPool>
#include <QRunnable>
#include <QPointer>
#include <QQueue>
#include <memory>
#include "video_utils.hpp"
#include <opencv2/highgui/highgui.hpp>


namespace bias 
{

    class HOGHOF;
    class beh_class;

    class ProcessScores : public QObject, public QRunnable, public Lockable<Empty>
    {

       Q_OBJECT

       public :

           bool detectStarted;
           QPointer<HOGHOF> HOGHOF_side;
           QPointer<HOGHOF> HOGHOF_front;

           ProcessScores();
           void stop();
           void enqueueFrameDataSender(FrameData frameData);
           void enqueueFrameDataReceiver(FrameData frameData);
           void setupHOGHOF_side();
           void setupHOGHOF_front();


       private :

           bool stopped_;
           bool ready_;
           int frameCount;

           videoBackend* vid_sde;
           videoBackend* vid_frt;
           cv::VideoCapture capture_sde;
           cv::VideoCapture capture_frt;
            
           QQueue<FrameData> senderImageQueue_;
           QQueue<FrameData> receiverImageQueue_;


           void run();
           void initHOGHOF(QPointer<HOGHOF> hoghof);
           void genFeatures(QPointer<HOGHOF> hoghof, int frameCount);

    };

}

#endif


















































