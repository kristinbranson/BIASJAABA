#include "image_grabber.hpp"
#include "camera_facade.hpp"
#include "exception.hpp"
#include <QPointer>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>


// CameraInfo
// ----------------------------------------------------------------------------

CameraInfo::CameraInfo()
{
    vendor = QString("");
    model = QString("");
    guid = QString("");
}

// ImageData
// ----------------------------------------------------------------------------
ImageData::ImageData()
{
    frameCount = 0;
    dateTime = 0.0;
}

// ImageGrabber
// ----------------------------------------------------------------------------

ImageGrabber::ImageGrabber(ImageGrabberParam param, QObject *parent) 
: QObject(parent) 
{ 
    param_ = param;
    stopped_ = false;
}


void ImageGrabber::stopCapture()
{
    stopped_ = true;
}


void ImageGrabber::run()
{ 

    if (param_.captureMode == QString("camera"))
    {
        runCaptureFromCamera();
    }
    else if (param_.captureMode == QString("file"))
    {
        runCaptureFromFile();
    }
    else
    {
        QString errorMsg = QString("Unable to start capture: unknown capture mode ");
        errorMsg += param_.captureMode;
        emit cameraSetupError(errorMsg);
    }

    CameraInfo emptyInfo;
    emit newCameraInfo(emptyInfo);
    emit stopped();
}

void ImageGrabber::runCaptureFromCamera()
{
    // Setup camera and start capture
    if (!setupCamera())
    {
        return;
    }

    try
    {
        cameraPtr_ -> startCapture();
    }
    catch (RuntimeError &runtimeError)
    {
        unsigned int errorId = runtimeError.id();
        QString errorMsg = QString("Unable to start capture: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return;
    }


    // Grab images
    ImageData imageData;

    while (!stopped_)
    {
        try
        {
            imageData.mat = cameraPtr_ -> grabImage();
        }
        catch (RuntimeError &runtimeError)
        {
            continue;
        }
        imageData.frameCount++; 
        QDateTime currentDateTime = QDateTime::currentDateTime();
        imageData.dateTime = double(currentDateTime.toMSecsSinceEpoch())*(1.0e-3);
        emit newImage(imageData);
    } 

    // Clean up
    try
    {
        cameraPtr_ -> stopCapture();
    }
    catch (RuntimeError &runtimeError)
    {
        unsigned int errorId = runtimeError.id();
        QString errorMsg = QString("Unable to start capture: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
    }

    try
    {
        cameraPtr_ -> disconnect();
    }
    catch (RuntimeError &runtimeError)
    {
        unsigned int errorId = runtimeError.id();
        QString errorMsg = QString("Unable to disconnect from camera: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
    }

}

void ImageGrabber::runCaptureFromFile()
{
    cv::VideoCapture fileCapture;

    // Open the capture input file
    try
    {
        fileCapture.open(param_.captureInputFile.toStdString());
    }
    catch(cv::Exception& exception)
    {
        QString errorMsg = QString("Error opening captureInputFile, ");
        errorMsg += param_.captureInputFile + QString(", - ");
        errorMsg += QString::fromStdString(exception.what());
        emit cameraSetupError(errorMsg);
        return;
    }
    if (!fileCapture.isOpened())
    {
        QString errorMsg = QString("Unable to open captureInputFile, ");
        errorMsg += param_.captureInputFile; 
        emit cameraSetupError(errorMsg);
        return;
    }

    // Get number of frames and fourcc
    unsigned int numFrames;
    int fourcc;
    try
    {
        numFrames = (unsigned int)(fileCapture.get(CV_CAP_PROP_FRAME_COUNT));
        fourcc = int(fileCapture.get(CV_CAP_PROP_FOURCC));
    }
    catch(cv::Exception& exception)
    {
        QString errorMsg = QString("Unable to get properties from captureInputFile, ");
        errorMsg += param_.captureInputFile + QString(", "); 
        errorMsg += QString::fromStdString(exception.what());
        emit cameraSetupError(errorMsg);
        return;
    }
    std::cout << "fourcc: " << fourcc << std::endl;
    std::cout << "numFrames: " << numFrames << std::endl;
    if (fourcc == 0)
    {
        // --------------------------------------------------------------------
        // TEMPORARY - currently having problems with DIB/raw formats need to 
        // fix this
        // --------------------------------------------------------------------
        QString errorMsg = QString("Fourcc code is equal to 0 - this is currently not supported");
        emit cameraSetupError(errorMsg);
        return;
    }

    // Read frame from input file at frameRate.
    unsigned long frameCount = 0;
    float sleepDt = 1.0e3/param_.frameRate;
    

    while ((!stopped_) && (frameCount < numFrames))
    {
        std::cout << param_.captureInputFile.toStdString() << ", frame = " << frameCount << std::endl;

        ImageData imageData;

        try
        {
            fileCapture >> imageData.mat;
        }
        catch (cv::Exception &exception)
        {
            QString errorMsg = QString("Unable to read frame %1: ").arg(frameCount);
            errorMsg += QString::fromStdString(exception.what());
            emit fileReadError(errorMsg);
            stopped_ = true;
            continue;
        }
        catch (...)
        {
            QString errorMsg = QString("Unable to read frame %1: ").arg(frameCount);
            errorMsg += QString("an uknown exception occured");
            emit fileReadError(errorMsg);
            stopped_ = true;
            continue;

        }
        if (imageData.mat.empty())
        {
            // This shouldn't happen, but just in case 
            // skip any frames that come back empty.
            continue;
        }


        imageData.frameCount = frameCount; 
        QDateTime currentDateTime = QDateTime::currentDateTime();
        imageData.dateTime = double(currentDateTime.toMSecsSinceEpoch())*(1.0e-3);
        emit newImage(imageData);

        ThreadHelper::msleep(sleepDt);
        frameCount++;
    }

    // Release file
    try
    {
        fileCapture.release();
    }
    catch(cv::Exception& exception)
    {
        QString errorMsg = QString("Error releasing captureInputFile, ");
        errorMsg += param_.captureInputFile + QString(", - ");
        errorMsg += QString::fromStdString(exception.what());
        emit cameraSetupError(errorMsg);
        return;
    }
}


bool ImageGrabber::setupCamera()
{
    CameraFinder cameraFinder;
    CameraPtrList cameraPtrList;  

    // Get list guids for all cameras found
    try
    { 
        cameraPtrList = cameraFinder.createCameraPtrList();
    }
    catch (bias::RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to enumerate cameras: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    // If no cameras found - error
    if (cameraPtrList.empty()) 
    {
        QString errorMsg = QString("No cameras found");
        emit cameraSetupError(errorMsg);
        return false;
    }

    // Connect to first camera
    cameraPtr_ = cameraPtrList.front();
    if (cameraPtr_ -> isConnected()) 
    {
        QString errorMsg = QString("Camera is already connected");
        emit cameraSetupError(errorMsg);
        return false;
    }
    try
    {
        cameraPtr_ -> connect();
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to connect to camera: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    CameraInfo cameraInfo;
    cameraInfo.vendor = QString::fromStdString(cameraPtr_ -> getVendorName());
    cameraInfo.model = QString::fromStdString(cameraPtr_ -> getModelName());
    cameraInfo.guid = QString::fromStdString((cameraPtr_ -> getGuid()).toString());
    emit newCameraInfo(cameraInfo);

    // Set video mode
    try
    {
        cameraPtr_ -> setVideoMode(VIDEOMODE_FORMAT7);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to set Video Mode: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    // Set trigger 
    try
    {
        cameraPtr_ -> setTriggerInternal();
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to set trigger: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    // Set frame rate
    PropertyInfo frameRateInfo;
    Property frameRateProp;
    
    try
    {
        frameRateInfo = cameraPtr_ -> getPropertyInfo(PROPERTY_TYPE_FRAME_RATE);
        frameRateProp = cameraPtr_ -> getProperty(PROPERTY_TYPE_FRAME_RATE);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to get framerate property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    if (param_.frameRate < frameRateInfo.minAbsoluteValue)
    {
        QString errorMsg = QString("framerate less than minimum allowed %1").arg(frameRateInfo.minAbsoluteValue);
        emit cameraSetupError(errorMsg);
        return false;

    }
    if (param_.frameRate > frameRateInfo.maxAbsoluteValue)
    {
        QString errorMsg = QString("framerate greater than maximum allowed %1").arg(frameRateInfo.maxAbsoluteValue);
        emit cameraSetupError(errorMsg);
        return false;
    }

    frameRateProp.absoluteControl = true;
    frameRateProp.absoluteValue = param_.frameRate;
    frameRateProp.autoActive = false;

    try
    {
        cameraPtr_ -> setProperty(frameRateProp);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to set framerate property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }



    // Set gain
    PropertyInfo gainInfo;
    Property gainProp;
    try
    {
        gainInfo = cameraPtr_ -> getPropertyInfo(PROPERTY_TYPE_GAIN);
        gainProp = cameraPtr_ -> getProperty(PROPERTY_TYPE_GAIN);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to get gain property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    if (param_.gain < gainInfo.minAbsoluteValue)
    {
        QString errorMsg = QString("gain less than minimum allowed %1").arg(gainInfo.minAbsoluteValue);
        emit cameraSetupError(errorMsg);
        return false;
    }
    if (param_.gain > gainInfo.maxAbsoluteValue)
    {
        QString errorMsg = QString("gain greater than maximum allowed %1").arg(gainInfo.minAbsoluteValue);
        emit cameraSetupError(errorMsg);
        return false;
    }

    gainProp.absoluteControl = true;
    gainProp.absoluteValue = param_.gain;
    gainProp.autoActive = false;

    try
    {
        cameraPtr_ -> setProperty(gainProp);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to set gain property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }
    

    // Set shutter
    PropertyInfo shutterInfo;
    Property shutterProp;
    try
    {
        shutterInfo = cameraPtr_ -> getPropertyInfo(PROPERTY_TYPE_SHUTTER);
        shutterProp = cameraPtr_ -> getProperty(PROPERTY_TYPE_SHUTTER);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to get shutter property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    if (param_.shutter < shutterInfo.minAbsoluteValue)
    {
        QString errorMsg = QString("shutter less than minimum allowed %1").arg(shutterInfo.minAbsoluteValue);
        emit cameraSetupError(errorMsg);
        return false;
    }
    if (param_.shutter > shutterInfo.maxAbsoluteValue)
    {
        QString errorMsg = QString("shutter greater than maximum allowed %1").arg(shutterInfo.minAbsoluteValue);
        emit cameraSetupError(errorMsg);
        return false;
    }

    shutterProp.absoluteControl = true;
    shutterProp.absoluteValue = param_.shutter;
    shutterProp.autoActive = false;

    try
    {
        cameraPtr_ -> setProperty(shutterProp);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to set shutter property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }
   


    // Set brightness
    PropertyInfo brightnessInfo;
    Property brightnessProp;
    try
    {
        brightnessInfo = cameraPtr_ -> getPropertyInfo(PROPERTY_TYPE_BRIGHTNESS);
        brightnessProp = cameraPtr_ -> getProperty(PROPERTY_TYPE_BRIGHTNESS);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to get brightness property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    if (param_.brightness < brightnessInfo.minValue)
    {
        QString errorMsg = QString("brightness less than minimum allowed %1").arg(brightnessInfo.minValue);
        emit cameraSetupError(errorMsg);
        return false;
    }
    if (param_.brightness > brightnessInfo.maxValue)
    {
        QString errorMsg = QString("brightness greater than maximum allowed %1").arg(brightnessInfo.minValue);
        emit cameraSetupError(errorMsg);
        return false;
    }

    brightnessProp.absoluteControl = false;
    brightnessProp.absoluteValue = param_.brightness;
    brightnessProp.autoActive = false;

    try
    {
        cameraPtr_ -> setProperty(brightnessProp);
    }
    catch (RuntimeError &runtimeError)
    {
        QString errorMsg = QString("Unable to set brightness property: ");
        errorMsg += QString::fromStdString(runtimeError.what());
        emit cameraSetupError(errorMsg);
        return false;
    }

    return true;
}
