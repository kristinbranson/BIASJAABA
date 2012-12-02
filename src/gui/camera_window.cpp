#include "camera_window.hpp"
#include "camera_facade.hpp"
#include "mat_to_qimage.hpp"
#include "stamped_image.hpp"
#include "lockable.hpp"
#include "lockable_queue.hpp"
#include "image_grabber.hpp"
#include "image_dispatcher.hpp"
#include <cstdlib>
#include <cmath>
#include <QtGui>
#include <QTimer>
#include <QThreadPool>
#include <QSignalMapper>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>

namespace bias
{

    // Public methods
    // ----------------------------------------------------------------------------------

    CameraWindow::CameraWindow(Guid cameraGuid, QWidget *parent) : QMainWindow(parent)
    {
        setupUi(this);
        connectWidgets();
        initialize(cameraGuid);
    }

    // Protected methods
    // ----------------------------------------------------------------------------------

    void CameraWindow::showEvent(QShowEvent *event)
    {
        resizeAllImageLabels();
    }

    void CameraWindow::resizeEvent(QResizeEvent *event)
    {
        resizeAllImageLabels();
    }

    void CameraWindow::closeEvent(QCloseEvent *event)
    {
        if (capturing_)
        {
            stopImageCapture();
            threadPoolPtr_ -> waitForDone();
        }

        if (connected_)
        {
            disconnectCamera();
        }

        event -> accept();
    }


    // Private slots
    // ----------------------------------------------------------------------------------

    void CameraWindow::connectButtonClicked()
    {
        (!connected_) ? connectCamera() : disconnectCamera();
    }


    void CameraWindow::startButtonClicked()
    { 
        (!capturing_) ? startImageCapture() : stopImageCapture();
    }
   

    void CameraWindow::updateImageDisplay()
    {
        // Timer update method for image displays. Note, this is getting a bit messy.
        
        QPointer<QWidget> currTabPtr = tabWidgetPtr_ -> currentWidget();

        bool isPreviewTab = (currTabPtr == previewTabPtr_);
        bool isPluginPreviewTab = (currTabPtr == pluginPreviewTabPtr_);
        bool isHistogramTab = (currTabPtr == histogramTabPtr_);

        // Get information from image dispatcher
        // ----------------------------------------------------------------
        imageDispatcherPtr_ -> acquireLock();

        cv::Mat imgMat = imageDispatcherPtr_ -> getImage();
        QImage img = matToQImage(imgMat);
        double fps = imageDispatcherPtr_ -> getFPS();
        double stamp = imageDispatcherPtr_ -> getTimeStamp();
        frameCount_ = imageDispatcherPtr_ -> getFrameCount();
        cv::Mat histMat = (isHistogramTab) ? calcHistogram(imgMat) : cv::Mat();
        cv::Size imgSize = imgMat.size();

        imageDispatcherPtr_ -> releaseLock();
        // ---------------------------------------------------------------

        // Update preview image
        if (isPreviewTab && !img.isNull()) 
        {
            previewPixmapOriginal_ = QPixmap::fromImage(img);
            updateImageLabel(previewImageLabelPtr_, previewPixmapOriginal_, true, true);
        }

        // TO DO ... Update plugin image
        if (isPluginPreviewTab)
        {
        }

        // Update histogram image
        if (isHistogramTab)
        {
            updateHistogramPixmap(histMat);
            updateImageLabel(histogramImageLabelPtr_, histogramPixmapOriginal_, false, false);
        }

        // Update statusbar message
        QString statusMsg("Capturing,  logging = ");
        statusMsg += boolToOnOffQString(logging_);
        statusMsg += QString(", timer = ");
        statusMsg += boolToOnOffQString(timer_);
        statusMsg += QString().sprintf(",  %dx%d", imgSize.width, imgSize.height);
        statusMsg += QString().sprintf(",  %1.1f fps", fps);
        statusbarPtr_ -> showMessage(statusMsg);

        // Update capture time
        setCaptureTimeLabel(stamp);
    }

    void CameraWindow::tabWidgetChanged(int index)
    {
        resizeAllImageLabels();
    }

    void CameraWindow::startImageCaptureError(unsigned int errorId, QString errorMsg)
    {
        stopImageCapture();
        QString msgTitle("Start Image Capture Error");
        QString msgText("Failed to start image capture\n\nError ID: ");
        msgText += QString::number(errorId);
        msgText += "\n\n";
        msgText += errorMsg;
        QMessageBox::critical(this, msgTitle, msgText);
    }


    void CameraWindow::stopImageCaptureError(unsigned int errorId, QString errorMsg)
    {
        QString msgTitle("Stop Image Capture Error");
        QString msgText("Failed to stop image capture\n\nError ID: ");
        msgText += QString::number(errorId);
        msgText += "\n\n";
        msgText += errorMsg;
        QMessageBox::critical(this, msgTitle, msgText);
    }


    void CameraWindow::actionFileLoadConfigTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionFileSaveconfigTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionFileCloseWindowTriggered()
    {
        // TO DO ... some sort of query before close if 
        // configuration has changed.
        close();
    }


    void CameraWindow::actionFileHideWindowTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionCameraInfoTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionCameraFormat7SettingsTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionLoggingEnabledTriggered()
    {
        logging_ = actionLoggingEnabledPtr_ -> isChecked();
        std::cout << "logging: " << boolToOnOffQString(logging_).toStdString() << std::endl;
    }


    void CameraWindow::actionLoggingVideoFileTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        QString videoFileFullPath = QFileDialog::getSaveFileName(
                this, 
                QString("Select Video File"),
                defaultSaveDir_.absolutePath()
                );

        std::cout << videoFileFullPath.toStdString() << std::endl;
    }
    

    void CameraWindow::actionLoggingSettingsTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionLoggingFormatTriggered()
    {
        // Get Format string
        QPointer<QAction> actionPtr = qobject_cast<QAction *>(sender());
        if (actionPtr != NULL) {
            QString formatString = actionPtr -> text();
            std:: cout << "logging format: " << formatString.toStdString() << std::endl;
        }
    }


    void CameraWindow::actionTimerEnabledTriggered()
    {
        timer_ = actionTimerEnabledPtr_ -> isChecked();
        std::cout << "timer: " << boolToOnOffQString(timer_).toStdString() << std::endl;
    }


    void CameraWindow::actionTimerSettingsTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionDisplayUpdateFreqTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionDisplayFlipVertTriggered()
    {
        flipVert_ = actionDisplayFlipVertPtr_ -> isChecked();
        std::cout << "flip vertical: " << boolToOnOffQString(flipVert_).toStdString() << std::endl;
    }


    void CameraWindow::actionDisplayFlipHorzTriggered()
    {
        flipHorz_ = actionDisplayFlipHorzPtr_ -> isChecked();
        std::cout << "flip horizontal: " << boolToOnOffQString(flipHorz_).toStdString() << std::endl;
    }


    void CameraWindow::actionDisplayRotTriggered()
    {
        QPointer<QAction> actionPtr = qobject_cast<QAction *>(sender());
        imageRotation_ = actionToRotationMap_[actionPtr];
        std:: cout << "display rotation: " << int(imageRotation_) << std::endl;
    }


    void CameraWindow::actionHelpUserManualTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }


    void CameraWindow::actionHelpAboutTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }

    void CameraWindow::actionPluginsSettingsTriggered()
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }

    void CameraWindow::actionVideoModeTriggered(int vidModeInt)
    {
        VideoMode vidMode = VideoMode(vidModeInt);
        std::cout << "videoMode: " << getVideoModeString(vidMode) << std::endl;
    }


    void CameraWindow::actionFrameRateTriggered(int frmRateInt)
    {
        FrameRate frmRate = FrameRate(frmRateInt);
        std::cout << "frameRate: " << getFrameRateString(frmRate) << std::endl;
    }


    void CameraWindow::actionPropertyTriggered(int propTypeInt)
    {
        PropertyType propType = PropertyType(propTypeInt);
        std::cout << "propertyType: " << getPropertyTypeString(propType) << std::endl;
    }


    // Private methods
    // -----------------------------------------------------------------------------------

    void CameraWindow::initialize(Guid guid)
    {
        connected_ = false;
        capturing_ = false;
        logging_ = false;
        timer_ = false;
        flipVert_ = false;
        flipHorz_ = false;
        imageRotation_ = IMAGE_ROTATION_0;

        imageDisplayFreq_ = DEFAULT_IMAGE_DISPLAY_FREQ;
        cameraPtr_ = std::make_shared<Lockable<Camera>>(guid);

        threadPoolPtr_ = new QThreadPool(this);
        newImageQueuePtr_ = std::make_shared<LockableQueue<StampedImage>>();

        setDefaultSaveDir();
        setupCameraMenu();
        setupLoggingMenu();
        setupDisplayMenu();
        setupImageDisplayTimer();
        setupImageLabels();

        tabWidgetPtr_ -> setCurrentWidget(previewTabPtr_);

        cameraPtr_ -> acquireLock();
        Guid cameraGuid = cameraPtr_ -> getGuid();
        cameraPtr_ -> releaseLock();

        QString windowTitle("BIAS Camera Window, Guid: ");
        windowTitle += QString::fromStdString(cameraGuid.toString());
        setWindowTitle(windowTitle);

        updateCameraInfoMessage();
        setCaptureTimeLabel(0.0);

        connectButtonPtr_ -> setText(QString("Connect"));
        startButtonPtr_ -> setText(QString("Start"));
        statusbarPtr_ -> showMessage(QString("Camera found, disconnected"));

        startButtonPtr_ -> setEnabled(false);
        connectButtonPtr_ -> setEnabled(true);

    }

    void CameraWindow::setupImageLabels()
    {
        QImage dummyImage;
        dummyImage = QImage(PREVIEW_DUMMY_IMAGE_SIZE,QImage::Format_RGB888);
        dummyImage.fill(QColor(Qt::gray).rgb());
        previewPixmapOriginal_ = QPixmap::fromImage(dummyImage);
        pluginPixmapOriginal_ = QPixmap::fromImage(dummyImage);

        dummyImage = QImage(DEFAULT_HISTOGRAM_IMAGE_SIZE,QImage::Format_RGB888);
        dummyImage.fill(QColor(Qt::gray).rgb());
        histogramPixmapOriginal_ = QPixmap::fromImage(dummyImage);

        updateImageLabel(previewImageLabelPtr_, previewPixmapOriginal_, true, true);
        updateImageLabel(pluginImageLabelPtr_, pluginPixmapOriginal_, true, false);
        updateImageLabel(histogramImageLabelPtr_, histogramPixmapOriginal_, false, false);
    }



    void CameraWindow::connectWidgets()
    {
        connect(
                startButtonPtr_, 
                SIGNAL(clicked()), 
                this, 
                SLOT(startButtonClicked())
                ); 

        connect(
                connectButtonPtr_, 
                SIGNAL(clicked()), 
                this, 
                SLOT(connectButtonClicked())
                );

        connect(
                actionCameraInfoPtr_, 
                SIGNAL(triggered()), 
                this, 
                SLOT(actionCameraInfoTriggered())
                );

        connect(
                actionCameraFormat7SettingsPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionCameraFormat7SettingsTriggered())
               );
       
        connect(
                actionLoggingEnabledPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionLoggingEnabledTriggered())
               );

        connect(
                actionLoggingVideoFilePtr_, 
                SIGNAL(triggered()),
                this,
                SLOT(actionLoggingVideoFileTriggered())
               );

        connect(
                actionLoggingSettingsPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionLoggingSettingsTriggered())
               );

        connect(
                actionLoggingFormatAVIPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionLoggingFormatTriggered())
               );

        connect(
                actionLoggingFormatFMFPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionLoggingFormatTriggered())
               );

        connect(
                actionLoggingFormatUFMFPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionLoggingFormatTriggered())
               );

        connect(
                actionTimerEnabledPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionTimerEnabledTriggered())
               );

        connect(
                actionTimerSettingsPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionTimerSettingsTriggered())
               );

        connect(
                actionDisplayUpdateFreqPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayUpdateFreqTriggered())
               );

        connect(
                actionDisplayFlipVertPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayFlipVertTriggered())
               );

        connect(
                actionDisplayFlipHorzPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayFlipHorzTriggered())
               );

        connect(
                actionDisplayRot0Ptr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayRotTriggered())
               );

        connect(
                actionDisplayRot90Ptr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayRotTriggered())
               );

        connect(
                actionDisplayRot180Ptr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayRotTriggered())
               );

        connect(
                actionDisplayRot270Ptr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionDisplayRotTriggered())
               );

        connect(
                actionHelpUserManualPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionHelpUserManualTriggered())
               );

        connect(
                actionHelpAboutPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionHelpAboutTriggered())
               );

        connect(
                actionFileLoadConfigPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionFileLoadConfigTriggered())
               );

        connect(
                actionFileSaveConfigPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionFileSaveconfigTriggered())
               );

        connect(
                actionFileCloseWindowPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionFileCloseWindowTriggered())
               );

        connect(
                actionFileHideWindowPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionFileHideWindowTriggered())
               );

        connect(
                actionPluginsSettingsPtr_,
                SIGNAL(triggered()),
                this,
                SLOT(actionPluginsSettingsTriggered())
               );

        connect(
                tabWidgetPtr_,
                SIGNAL(currentChanged(int)),
                this,
                SLOT(tabWidgetChanged(int))
               );
    }


    void CameraWindow::setDefaultSaveDir()
    {
#ifdef WIN32
        defaultSaveDir_ = QDir(QString(getenv("USERPROFILE")));
#else
        defaultSaveDir_ = QDir(QString(getenv("HOME")));
#endif 
        if (!defaultSaveDir_.exists())
        {
            // Something is wrong with home directory set to root.
            defaultSaveDir_ = QDir(QDir::rootPath());
        }
    }


    void CameraWindow::setupImageDisplayTimer()
    {
        imageDisplayTimerPtr_ = new QTimer(this);
        connect(
                imageDisplayTimerPtr_, 
                SIGNAL(timeout()), 
                this, 
                SLOT(updateImageDisplay())
                );
    }


    void CameraWindow::setupCameraMenu()
    {
        videoModeActionGroupPtr_ = new QActionGroup(menuCameraVideoModePtr_);
        frameRateActionGroupPtr_ = new QActionGroup(menuCameraFrameRatePtr_);

        videoModeSignalMapperPtr_ = new QSignalMapper(menuCameraVideoModePtr_);
        frameRateSignalMapperPtr_ = new QSignalMapper(menuCameraFrameRatePtr_);
        propertiesSignalMapperPtr_ = new QSignalMapper(menuCameraPropertiesPtr_);

        connect(
                videoModeSignalMapperPtr_,
                SIGNAL(mapped(int)),
                this,
                SLOT(actionVideoModeTriggered(int))
               );

        connect(
                frameRateSignalMapperPtr_,
                SIGNAL(mapped(int)),
                this,
                SLOT(actionFrameRateTriggered(int))
               );

        connect(
                propertiesSignalMapperPtr_,
                SIGNAL(mapped(int)),
                this,
                SLOT(actionPropertyTriggered(int))
               );

        cameraTriggerActionGroupPtr_ = new QActionGroup(menuCameraPtr_);
        cameraTriggerActionGroupPtr_ -> addAction(actionCameraTriggerInternalPtr_);
        cameraTriggerActionGroupPtr_ -> addAction(actionCameraTriggerExternalPtr_);

        setMenuChildrenEnabled(menuCameraPtr_, false);

        // TO DO .... temporary, disable external trigger option
        // -----------------------------------------------------
        actionCameraTriggerExternalPtr_ -> setEnabled(false);
        // -----------------------------------------------------
    }


    void CameraWindow::setupLoggingMenu()
    {
        loggingFormatActionGroupPtr_ = new QActionGroup(menuLoggingFormatPtr_);
        loggingFormatActionGroupPtr_ -> addAction(actionLoggingFormatAVIPtr_);
        loggingFormatActionGroupPtr_ -> addAction(actionLoggingFormatFMFPtr_);
        loggingFormatActionGroupPtr_ -> addAction(actionLoggingFormatUFMFPtr_);
        actionLoggingFormatAVIPtr_ -> setChecked(true);
        actionLoggingFormatFMFPtr_ -> setChecked(false);
        actionLoggingFormatUFMFPtr_ -> setChecked(false);
    }


    void CameraWindow::setupDisplayMenu() 
    {
        setupDisplayRotMenu();
        setupDisplayOrientMenu();
    }


    void CameraWindow::setupDisplayOrientMenu()
    {

        actionDisplayFlipVertPtr_ -> setChecked(flipVert_);
        actionDisplayFlipHorzPtr_ -> setChecked(flipHorz_);
    }
    

    void CameraWindow::setupDisplayRotMenu()
    {
        rotationActionGroupPtr_ =  new QActionGroup(menuDisplayRotPtr_);
        rotationActionGroupPtr_ -> addAction(actionDisplayRot0Ptr_);
        rotationActionGroupPtr_ -> addAction(actionDisplayRot90Ptr_);
        rotationActionGroupPtr_ -> addAction(actionDisplayRot180Ptr_);
        rotationActionGroupPtr_ -> addAction(actionDisplayRot270Ptr_);

        actionToRotationMap_[actionDisplayRot0Ptr_] =  IMAGE_ROTATION_0;
        actionToRotationMap_[actionDisplayRot90Ptr_] = IMAGE_ROTATION_90;
        actionToRotationMap_[actionDisplayRot180Ptr_] = IMAGE_ROTATION_180;
        actionToRotationMap_[actionDisplayRot270Ptr_] = IMAGE_ROTATION_270; 
        
        QMap<QAction*, ImageRotationType>:: iterator mapIt;
        for (
                mapIt = actionToRotationMap_.begin();
                mapIt != actionToRotationMap_.end();
                mapIt++
            )
        {
            ImageRotationType rotType = *mapIt;
            QAction *actionPtr = mapIt.key();
            if (rotType == imageRotation_)
            {
                actionPtr -> setChecked(true);
            }
            else
            {
                actionPtr -> setChecked(false);
            }
        }
    }


    void CameraWindow::updateImageLabel(
            QLabel *imageLabelPtr, 
            QPixmap &pixmapOriginal,
            bool flipAndRotate,
            bool addFrameCount
            )
    {
        // Updates pixmap of image on Qlabel - sizing based on QLabel size

        QPixmap pixmapScaled =  pixmapOriginal.scaled(
                imageLabelPtr -> size(),
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
                );
        
        // Flip and rotate pixmap if required
        if (flipAndRotate) {
            if ((imageRotation_ != IMAGE_ROTATION_0) || flipVert_ || flipHorz_ )
            {
                QTransform transform;
                transform.rotate(-1.0*float(imageRotation_));
                if (flipVert_)
                {
                    transform.scale(1.0,-1.0);
                }
                if (flipHorz_) 
                {
                    transform.scale(-1.0,1.0);
                }
                pixmapScaled = pixmapScaled.transformed(transform);
            }
        }

        // Add frame count
        if (addFrameCount && (frameCount_ > 0))
        {
            QPainter painter(&pixmapScaled);
            QString msg;  
            msg.sprintf("%d",frameCount_);
            painter.setPen(QColor(0,220,0));
            painter.drawText(5,12, msg);
        }

        imageLabelPtr -> setPixmap(pixmapScaled);
    }


    void CameraWindow::resizeImageLabel( 
            QLabel *imageLabelPtr, 
            QPixmap &pixmapOriginal, 
            bool flipAndRotate,
            bool addFrameCount
            )
    {
        // Determines if resize of pixmap of image on Qlabel is required and 
        // if so resizes the pixmap.
        
        if (pixmapOriginal.isNull() || ((imageLabelPtr -> pixmap()) == 0))  
        {
            return;
        }
        QSize sizeImageLabel = imageLabelPtr -> size();
        QSize sizeAdjusted = pixmapOriginal.size();
        sizeAdjusted.scale(sizeImageLabel, Qt::KeepAspectRatio);
        QSize sizeImageLabelPixmap = imageLabelPtr -> pixmap() -> size();
        if (sizeImageLabelPixmap != sizeAdjusted) 
        {
            updateImageLabel(
                    imageLabelPtr,
                    pixmapOriginal,
                    flipAndRotate,
                    addFrameCount
                    );
        }
    }

    void CameraWindow::resizeAllImageLabels()
    { 
        resizeImageLabel(previewImageLabelPtr_, previewPixmapOriginal_, true);
        resizeImageLabel(pluginImageLabelPtr_, pluginPixmapOriginal_, false);
        resizeImageLabel(histogramImageLabelPtr_, histogramPixmapOriginal_, false);
    }

    void CameraWindow::updateHistogramPixmap(cv::Mat hist)
    {
        QImage dummyImage = QImage(DEFAULT_HISTOGRAM_IMAGE_SIZE,QImage::Format_RGB888);
        dummyImage.fill(QColor(Qt::gray).rgb());
        histogramPixmapOriginal_ = QPixmap::fromImage(dummyImage);

        QPainter painter(&histogramPixmapOriginal_);
        painter.setPen(QColor(50,50,50));
        cv::Size histSize = hist.size();

        float histImageMaxY = float(DEFAULT_HISTOGRAM_IMAGE_SIZE.height() - 1.0);

        for (int i=0; i<histSize.height; i++) 
        {
            int y0 = int(histImageMaxY);
            int y1 = int(histImageMaxY - hist.at<float>(0,i));
            painter.drawLine(i,y0,i,y1);
        }

    }

    void CameraWindow::connectCamera() 
    {
        bool error = false;
        unsigned int errorId;
        QString errorMsg;

        cameraPtr_ -> acquireLock();
        try
        {
            cameraPtr_ -> connect();
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_ -> releaseLock();

        if (error)
        {
            QString msgTitle("Connection Error");
            QString msgText("Failed to connect camera:\n\nError ID: ");
            msgText += QString::number(errorId);
            msgText += "\n\n";
            msgText += errorMsg;
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }
        connected_ = true;
        connectButtonPtr_ -> setText(QString("Disconnect"));
        statusbarPtr_ -> showMessage(QString("Connected, Stopped"));
        startButtonPtr_ -> setEnabled(true);
        menuCameraPtr_ -> setEnabled(true);
        updateCameraMenu();
        updateCameraInfoMessage();

    }


    void CameraWindow::disconnectCamera()
    {
        bool error = false;
        unsigned int errorId;
        QString errorMsg; 

        if (capturing_) 
        {
            stopImageCapture();
        }

        cameraPtr_ -> acquireLock();
        try
        {
            cameraPtr_ -> disconnect();
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_ -> releaseLock();

        if (error)
        {
            QString msgTitle("Disconnection Error");
            QString msgText("Failed to disconnect camera:\n\nError ID: ");
            msgText += QString::number(errorId);
            msgText += "\n\n";
            msgText += errorMsg;
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }
        connected_ = false;
        connectButtonPtr_ -> setText(QString("Connect"));
        statusbarPtr_ -> showMessage(QString("Disconnected"));
        startButtonPtr_ -> setEnabled(false);
        menuCameraPtr_ -> setEnabled(false);
        updateCameraInfoMessage();
        setCaptureTimeLabel(0.0);
    }

    
    void CameraWindow::startImageCapture() 
    {
        if (!connected_)
        {
            QString msgTitle("Capture Error");
            QString msgText("Unable to start image capture: not connected");
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }


        imageGrabberPtr_ = new ImageGrabber(cameraPtr_, newImageQueuePtr_);
        imageDispatcherPtr_ = new ImageDispatcher(newImageQueuePtr_);

        connect(
                imageGrabberPtr_, 
                SIGNAL(startCaptureError(unsigned int, QString)),
                this,
                SLOT(startImageCaptureError(unsigned int, QString))
               );

        connect(
                imageGrabberPtr_,
                SIGNAL(stopCaptureError(unsigned int, QString)),
                this,
                SLOT(stopImageCaptureError(unsigned int, QString))
               );

        threadPoolPtr_ -> start(imageGrabberPtr_);
        threadPoolPtr_ -> start(imageDispatcherPtr_);

        unsigned int imageDisplayDt = int(1000.0/imageDisplayFreq_);
        imageDisplayTimerPtr_ -> start(imageDisplayDt);
        startButtonPtr_ -> setText(QString("Stop"));
        connectButtonPtr_ -> setEnabled(false);
        statusbarPtr_ -> showMessage(QString("Capturing"));
        capturing_ = true;

        updateLoggingMenu();
        updateTimerMenu();

    }


    void CameraWindow::stopImageCapture()
    {
        if (!connected_)
        {
            QString msgTitle("Capture Error");
            QString msgText("Unable to stop image capture: not connected");
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }

        // Note, image grabber and dispatcher are destroyed by the 
        // threadPool when their run methods exit.
        imageDisplayTimerPtr_ -> stop();

        if (!imageGrabberPtr_.isNull())
        {
            imageGrabberPtr_ -> acquireLock();
            imageGrabberPtr_ -> stop();
            imageGrabberPtr_ -> releaseLock();
        }

        if (!imageDispatcherPtr_.isNull())
        {
            imageDispatcherPtr_ -> acquireLock();
            imageDispatcherPtr_ -> stop();
            imageDispatcherPtr_ -> releaseLock();
        }

        newImageQueuePtr_ -> clear();
        startButtonPtr_ -> setText(QString("Start"));
        connectButtonPtr_ -> setEnabled(true);
        statusbarPtr_ -> showMessage(QString("Connected, Stopped"));
        capturing_ = false;

        updateLoggingMenu();
        updateTimerMenu();
    }


    void CameraWindow::updateCameraInfoMessage()
    {
        if (connected_) 
        {
            cameraPtr_ -> acquireLock();
            QString vendorName = QString::fromStdString(
                    cameraPtr_ -> getVendorName()
                    );
            QString modelName = QString::fromStdString( 
                    cameraPtr_ -> getModelName()
                    );
            cameraPtr_ -> releaseLock();
            setCameraInfoMessage(vendorName, modelName);
        }
        else
        {
            setCameraInfoMessage("_____", "_____");
        }

    }

    
    void CameraWindow::setCameraInfoMessage(QString vendorName, QString modelName)
    {
        QString cameraInfoString("Camera:  ");
        cameraInfoString += vendorName;
        cameraInfoString += QString(",  ");
        cameraInfoString += modelName; 
        cameraInfoLabelPtr_ -> setText(cameraInfoString);
    }


    void CameraWindow::setMenuChildrenEnabled(QWidget *parentWidgetPtr, bool value)
    {
        QList<QMenu *> childList = parentWidgetPtr -> findChildren<QMenu *>();
        QList<QMenu *>::iterator childIt;
        for (childIt=childList.begin(); childIt!=childList.end(); childIt++)
        {
            QPointer<QMenu> menuPtr = *childIt;
            menuPtr -> setEnabled(value);
        }

        QList<QAction *> actionList = parentWidgetPtr -> actions();
        QList<QAction *>::iterator actionIt;
        for (actionIt=actionList.begin(); actionIt!=actionList.end(); actionIt++)
        {
            QPointer<QAction> actionPtr = *actionIt;
            actionPtr -> setEnabled(value);
        }
    } 


    void CameraWindow::updateCameraMenu()
    {
        if (connected_)
        {
            setMenuChildrenEnabled(menuCameraPtr_, true);
            populateVideoModeMenu();
            populateFrameRateMenu();
            populatePropertiesMenu();

            // TO DO .. temporarily disable format7 settings
            // ------------------------------------------------
            actionCameraFormat7SettingsPtr_ -> setEnabled(false);
            // ------------------------------------------------
        }
        else 
        {
            setMenuChildrenEnabled(menuCameraPtr_, true);
        }

        updateCameraTriggerMenu();
        updateCameraVideoModeMenu();
        updateCameraFrameRateMenu();
        updateCameraPropertiesMenu();
    }


    void CameraWindow::populateVideoModeMenu()
    {
        bool error = false;
        unsigned int errorId;
        QString errorMsg;
        VideoMode currentVideoMode;
        VideoModeList videoModeList;

        if (!connected_) { return; }

        // Remove any existing actions
        deleteMenuActions(menuCameraVideoModePtr_, videoModeActionGroupPtr_);

        // Get list of allowed videomodes from camera 
        cameraPtr_ -> acquireLock();
        try
        {
            currentVideoMode = cameraPtr_ -> getVideoMode();
            videoModeList = cameraPtr_ -> getAllowedVideoModes(); 
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_ -> releaseLock();

        if (error) 
        {
            QString msgTitle("Camera Query Error");
            QString msgText("Failed to read allowed video modes from camera:");
            msgText += QString("\n\nError ID: ") + QString::number(errorId);
            msgText += "\n\n";
            msgText += errorMsg;
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }

        // Add action to menu for each allowed video mode
        VideoModeList::iterator modeIt;
        for (modeIt=videoModeList.begin(); modeIt!=videoModeList.end(); modeIt++)
        {
            VideoMode mode = *modeIt;
            QString modeString = QString::fromStdString(getVideoModeString(mode));
            QPointer<QAction> modeActionPtr = 
                menuCameraVideoModePtr_ -> addAction(modeString);
            videoModeActionGroupPtr_ -> addAction(modeActionPtr);
            modeActionPtr -> setCheckable(true);
            
            connect( 
                    modeActionPtr, 
                    SIGNAL(triggered()), 
                    videoModeSignalMapperPtr_, 
                    SLOT(map())
                   );
            videoModeSignalMapperPtr_ -> setMapping(modeActionPtr, int(mode));

            if (mode == currentVideoMode)
            {
                modeActionPtr -> setChecked(true);
            }
            else
            {
                modeActionPtr -> setChecked(false);
            }
            // --------------------------------------------------------------------
            // TO DO ... temporary, currently don't allow video mode to be changed
            // ---------------------------------------------------------------------
            modeActionPtr -> setEnabled(false);
        }
    }


    void CameraWindow::populateFrameRateMenu()
    {
        bool error = false;
        unsigned int errorId;
        QString errorMsg;
        FrameRateList allowedRateList;
        FrameRate currentFrameRate;
        VideoMode currentVideoMode;

        if (!connected_) { return; }

        // Remove any existing actions from menu
        deleteMenuActions(menuCameraFrameRatePtr_, frameRateActionGroupPtr_);

        // Get list of allowed framerates from camera 
        cameraPtr_ -> acquireLock();
        try
        {
            currentFrameRate = cameraPtr_ -> getFrameRate();
            currentVideoMode = cameraPtr_ -> getVideoMode();
            allowedRateList = cameraPtr_ -> getAllowedFrameRates(currentVideoMode); 
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_ -> releaseLock();

        if (error) 
        {
            QString msgTitle("Camera Query Error");
            QString msgText("Failed to read frame rates from camera:");  
            msgText += QString("\n\nError ID: ") + QString::number(errorId);
            msgText += "\n\n";
            msgText += errorMsg;
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }

        // Add action to menu for each allowed frame rate
        FrameRateList::iterator rateIt;
        for (rateIt=allowedRateList.begin(); rateIt!=allowedRateList.end(); rateIt++)
        {
            FrameRate rate = *rateIt;
            QString rateString = QString::fromStdString(getFrameRateString(rate));
            QPointer<QAction> rateActionPtr = 
                menuCameraFrameRatePtr_ -> addAction(rateString);
            frameRateActionGroupPtr_ -> addAction(rateActionPtr);
            rateActionPtr -> setCheckable(true);

            connect( 
                    rateActionPtr, 
                    SIGNAL(triggered()), 
                    frameRateSignalMapperPtr_, 
                    SLOT(map())
                   );
            frameRateSignalMapperPtr_ -> setMapping(rateActionPtr, int(rate));

            if (rate == currentFrameRate)
            {
                rateActionPtr -> setChecked(true);
            }
            else
            {
                rateActionPtr -> setChecked(false);
            } 
            // --------------------------------------------------------------------
            // TO DO ... temporary, currently don't allow frame rate to be changed
            // ---------------------------------------------------------------------
            rateActionPtr -> setEnabled(false);
        }
    }


    void CameraWindow::populatePropertiesMenu()
    {
        bool error = false;
        unsigned int errorId;
        QString errorMsg;
        PropertyList propList;
        PropertyInfoMap propInfoMap;

        if (!connected_) { return; }

        // Remove any existing actions from menu
        deleteMenuActions(menuCameraPropertiesPtr_);

        // Get list of properties from camera 
        cameraPtr_ -> acquireLock();
        try
        {
            propList = cameraPtr_ -> getListOfProperties();
            propInfoMap = cameraPtr_ -> getMapOfPropertyInfos();
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_ -> releaseLock();

        if (error) 
        {
            QString msgTitle("Camera Query Error");
            QString msgText("Failed to read properties from camera:");  
            msgText += QString("\n\nError ID: ") + QString::number(errorId);
            msgText += "\n\n";
            msgText += errorMsg;
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }

        // Action action to menu for each property in list
        PropertyList::iterator propIt;
        for (propIt=propList.begin(); propIt!=propList.end(); propIt++)
        {
            Property prop = *propIt;
            PropertyInfo propInfo = propInfoMap[prop.type];
            if (prop.present)
            {
                //prop.print();
                //propInfo.print();
                std::string propStringStd = getPropertyTypeString(prop.type);
                QString propString = QString::fromStdString(propStringStd);
                QPointer<QAction> propActionPtr = 
                    menuCameraPropertiesPtr_ -> addAction(propString);

                connect( 
                        propActionPtr, 
                        SIGNAL(triggered()), 
                        propertiesSignalMapperPtr_, 
                        SLOT(map())
                        );
                propertiesSignalMapperPtr_ -> setMapping(propActionPtr, int(prop.type));

                // --------------------------------------------------------------
                // TO DO ... temporary
                // ---------------------------------------------------------------
                //propActionPtr -> setEnabled(false);
            }
        }
    }


    void CameraWindow::updateCameraVideoModeMenu()
    {
    }


    void CameraWindow::updateCameraFrameRateMenu()
    {
    }


    void CameraWindow::updateCameraPropertiesMenu()
    {
    }


    void CameraWindow::updateCameraTriggerMenu()
    {
        bool error = false;
        unsigned int errorId;
        QString errorMsg;
        TriggerType trigType;

        if (!connected_) 
        {
            return;
        }

        cameraPtr_ -> acquireLock();
        try
        {
            trigType = cameraPtr_ -> getTriggerType();
        }
        catch (RuntimeError &runtimeError)
        {
            error = true;
            errorId = runtimeError.id();
            errorMsg = QString::fromStdString(runtimeError.what());
        }
        cameraPtr_ -> releaseLock();

        if (error)
        {
            QString msgTitle("Camera Query Error");
            QString msgText("Failed to read trigger type from camera:\n\nError ID: ");
            msgText += QString::number(errorId);
            msgText += "\n\n";
            msgText += errorMsg;
            QMessageBox::critical(this, msgTitle, msgText);
            return;
        }

        if (trigType == TRIGGER_INTERNAL)
        {
            actionCameraTriggerInternalPtr_ -> setChecked(true);
            actionCameraTriggerExternalPtr_ -> setChecked(false);
        }
        else 
        {
            actionCameraTriggerInternalPtr_ -> setChecked(false);
            actionCameraTriggerExternalPtr_ -> setChecked(true);
        }
    }


    void CameraWindow::updateLoggingMenu() 
    {
        if (capturing_)
        {
            setMenuChildrenEnabled(menuLoggingPtr_, false);
        }
        else
        {
            setMenuChildrenEnabled(menuLoggingPtr_, true);
        }
    }


    void CameraWindow::updateTimerMenu()
    {
        if (capturing_)
        {
            setMenuChildrenEnabled(menuTimerPtr_, false);
        }
        else
        {
            setMenuChildrenEnabled(menuTimerPtr_, true);
        }
    }


    void CameraWindow::deleteMenuActions(QMenu *menuPtr, QActionGroup *actionGroupPtr)
    {
        QList<QAction *> actionList = menuPtr -> actions();
        QList<QAction *>::iterator actionIt;
        for (actionIt=actionList.begin(); actionIt!=actionList.end(); actionIt++)
        {
            QPointer<QAction> actionPtr = *actionIt;
            if (actionGroupPtr != NULL)
            {
                actionGroupPtr -> removeAction(actionPtr);

            }
            menuPtr -> removeAction(actionPtr);
        }
    }


    void CameraWindow::setCaptureTimeLabel(double timeStamp)
    {
        QString stampString = timeStampToQString(timeStamp); 
        captureTimeLabelPtr_ -> setText(stampString);
    }

    // Development
    // ----------------------------------------------------------------------------------
    cv::Mat CameraWindow::calcHistogram(cv::Mat mat)
    {
        // -----------------------------------------------------------------------------
        // TO DO  - only for 8bit black and white cameras needs modification for color.
        // -----------------------------------------------------------------------------
        int channels[] = {0};
        int histSize[] = {256};
        float dimRange[] = {0,256}; // need to be set based on image type.
        const float *ranges[] = {dimRange};
        double minVal = 0;
        double maxVal = 0;
        double histImageMaxY = DEFAULT_HISTOGRAM_IMAGE_SIZE.height() - 1.0;

        cv::Mat hist;
        cv::calcHist(&mat, 1, channels, cv::Mat(), hist, 1, histSize, ranges, true, false);
        minMaxLoc(hist,&minVal,&maxVal,NULL,NULL);
        hist = hist*(float(histImageMaxY)/float(maxVal));
        return hist;
    }


    // Utility functions
    // ----------------------------------------------------------------------------------

    QString boolToOnOffQString(bool value)
    {
        return (value) ? QString("on") : QString("off");
    }


    QString timeStampToQString(double timeStamp)
    {
        double hrs, min, sec;
        double rem = timeStamp; 

        hrs = std::floor(timeStamp/3600.0);
        rem -= hrs;
        min = std::floor(rem/60.0);
        rem -= min;
        sec = std::floor(rem);

        QString timeString;
        timeString.sprintf("%02d:%02d:%02d", int(hrs), int(min), int(sec));
        return timeString;
    }

} // namespace bias

       

