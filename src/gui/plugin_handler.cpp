#include <plugin_handler.hpp>
#include <QThread>
#include <iostream>
#include <sstream>
#include "affinity.hpp"
#include "stamped_image.hpp"
#include <QtDebug>
//#include "timer.h"

namespace bias
{ 
    const unsigned int PluginHandler::MAX_IMAGE_QUEUE_SIZE = 500;

    PluginHandler::PluginHandler(QObject *parent) : QObject(parent)
    {
        QPointer<BiasPlugin> emptyPluginPtr;
        initialize(0,NULL,emptyPluginPtr);
    }


    PluginHandler::PluginHandler(
            unsigned int cameraNumber,
            std::shared_ptr<LockableQueue<StampedImage>> pluginImageQueuePtr,
            QObject *parent
            ) : QObject(parent)
    {
        QPointer<BiasPlugin> emptyPluginPtr;
        initialize(cameraNumber, pluginImageQueuePtr, emptyPluginPtr);
    }


    PluginHandler::PluginHandler(
            unsigned int cameraNumber,
            std::shared_ptr<LockableQueue<StampedImage>> pluginImageQueuePtr,
            BiasPlugin *pluginPtr,
            QObject *parent
            ) : QObject(parent)
    {
        QPointer<BiasPlugin> emptyPluginPtr;
        initialize(cameraNumber, pluginImageQueuePtr, emptyPluginPtr);
    }


    void PluginHandler::setCameraNumber(unsigned int cameraNumber)
    {
        cameraNumber_ = cameraNumber;
    } 

    void PluginHandler::setImageQueue(std::shared_ptr<LockableQueue<StampedImage>> pluginImageQueuePtr,
                                      std::shared_ptr<LockableQueue<unsigned int>> skippedFramesPluginPtr)
    {
        pluginImageQueuePtr_ = pluginImageQueuePtr;
        skippedFramesPluginPtr_ = skippedFramesPluginPtr;
        setReadyState();
    }


    void PluginHandler::setPlugin(BiasPlugin *pluginPtr)
    {
        pluginPtr_ = pluginPtr;
        setReadyState();

        // setImage queue for set pluginPtr
        if(pluginImageQueuePtr_ != nullptr)
        {
            pluginPtr_->setImageQueue(pluginImageQueuePtr_, skippedFramesPluginPtr_);     
        }

    }


    void PluginHandler::initialize(
            unsigned int cameraNumber,
            std::shared_ptr<LockableQueue<StampedImage>> pluginImageQueuePtr,
            BiasPlugin *pluginPtr
            )
    {
        ready_ = false;
        stopped_ = true;
        setCameraNumber(cameraNumber);
        setImageQueue(pluginImageQueuePtr, skippedFramesPluginPtr_);
        setPlugin(pluginPtr);

        // Plugin setup actions
        //std::cout << "plugin: setup" << std::endl;
    }


    void PluginHandler::stop()
    {
        stopped_ = true;
    }


    cv::Mat PluginHandler::getImage() const
    {
        cv::Mat currentImage;
        if (!pluginPtr_.isNull())
        {
            currentImage = pluginPtr_ -> getCurrentImage();
        }
        return currentImage;
    }


    void PluginHandler::setReadyState()
    {
        if ((pluginImageQueuePtr_ != NULL) && (!pluginPtr_.isNull()))
        {
            ready_ = true;
        }
        else
        {
            ready_ = false;
        }
    }


    void PluginHandler::run()
    {

        bool done = false;

        if (!ready_) 
        { 
            return; 
        }

        QThread *thisThread = QThread::currentThread();
        thisThread -> setPriority(QThread::NormalPriority);
        ThreadAffinityService::assignThreadAffinity(false,cameraNumber_);

        acquireLock();
        stopped_ = false;
        releaseLock();

        //GpuTimer timer;
        //timer.Start();
        while (!done)
        {
            //QList<StampedImage> frameList;

            // Grab frame from image queue
            pluginImageQueuePtr_ -> acquireLock();
            pluginImageQueuePtr_ -> waitIfEmpty();
            if (pluginImageQueuePtr_ -> empty())
            {
                pluginImageQueuePtr_ -> releaseLock();
                break;
            }

            /*while ( !(pluginImageQueuePtr_ ->  empty()) )
            {
                StampedImage stampedImage = pluginImageQueuePtr_ -> front();
                frameList.append(stampedImage);
                pluginImageQueuePtr_ -> pop();
            }*/


            pluginImageQueuePtr_ -> releaseLock();

            // Process Frame with plugin
            if (!pluginPtr_.isNull())
            { 
                //pluginPtr_ -> processFrames(frameList);
                pluginPtr_ -> processFrames();
            }

            acquireLock();
            done = stopped_;
            releaseLock();

        } // while (!done)
        //timer.Stop();
        //std::cout << timer.Elapsed()/1000 << std::endl;
        
        // Plugin clean up actions
        //std::cout << "plugin: clean up" << std::endl;

    } // PlugingHandler::run()

    

} // namespace bias;
