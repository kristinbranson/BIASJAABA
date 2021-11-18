#ifdef WITH_SPIN
#ifndef BIAS_CAMERA_DEVICE_SPIN_HPP
#define BIAS_CAMERA_DEVICE_SPIN_HPP

#include <map>
#include <string>
#include <functional>
#include <opencv2/core/core.hpp>

#include "utils.hpp"
#include "camera_device.hpp"
#include "camera_info_spin.hpp"
#include "property.hpp"
#include "SpinnakerC.h"

#include "node_map_spin.hpp"
#include "base_node_spin.hpp"
#include "string_node_spin.hpp"
#include "enum_node_spin.hpp"
#include "entry_node_spin.hpp"
#include "number_node_spin.hpp"
#include "float_node_spin.hpp"
#include "integer_node_spin.hpp"
#include "bool_node_spin.hpp"
#include "enum_node_spin.hpp"
#include "command_node_spin.hpp"

//test
//#ifdef WIN32
#include "win_time.hpp"
//#endif
#include "stamped_image.hpp"
#include <QPointer>
#include "NIDAQUtils.hpp"
#include "test_config.hpp"

namespace bias {



    class CameraDevice_spin : public CameraDevice
    {
        public:

            CameraDevice_spin(); 
            explicit CameraDevice_spin(Guid guid);
            virtual ~CameraDevice_spin();

            virtual CameraLib getCameraLib();

            virtual void connect();
            virtual void disconnect();
            
            virtual void startCapture();
            virtual void stopCapture();

            virtual cv::Mat grabImage();
            virtual void grabImage(cv::Mat &image);

            virtual bool isColor();
            
            virtual bool isSupported(VideoMode vidMode, FrameRate frmRate);
            virtual bool isSupported(ImageMode imgMode);
            virtual size_t getNumberOfImageMode();

            virtual VideoMode getVideoMode();
            virtual FrameRate getFrameRate();
            virtual ImageMode getImageMode();

            virtual VideoModeList getAllowedVideoModes();
            virtual FrameRateList getAllowedFrameRates(VideoMode vidMode);
            virtual ImageModeList getAllowedImageModes();

            virtual PropertyInfo getPropertyInfo(PropertyType propType);
            virtual Property getProperty(PropertyType propType);
            virtual void setProperty(Property prop);
            
            //virtual void setFormat7ImageMode(ImageMode imgMode); // TO DO //

            virtual Format7Settings getFormat7Settings();
            virtual Format7Info getFormat7Info(ImageMode imgMode);
            
            virtual bool validateFormat7Settings(Format7Settings settings);
            virtual void setFormat7Configuration(Format7Settings settings, float percentSpeed);
            
            virtual PixelFormatList getListOfSupportedPixelFormats(ImageMode imgMode);

            virtual void setTriggerInternal();
            virtual void setTriggerExternal();
            virtual TriggerType getTriggerType();

            virtual std::string getVendorName();
            virtual std::string getModelName();

            virtual TimeStamp getImageTimeStamp();
            
            virtual std::string toString();

            virtual void printGuid();
            
            virtual void printInfo();

            virtual TimeStamp getDeviceTimeStamp();
            virtual void initCounter();

            void develExpProps();

            // Constants
            // --------------------------------------------------------
            // Artificial shutter limits to make GUI more usable.
            static constexpr double MinAllowedShutterUs = 100.0;
            static constexpr double MaxAllowedShutterUs = 200000.0; 

            // Number of times auto mode is set to "once". Seems to need to be called 
            // more than once  in order to reach a stable value.
            static constexpr int AutoOnePushSetCount = 10;

            //test
            virtual TimeStamp cameraOffsetTime();
            virtual TimeStamp getCPUtime();
            TimeStamp cam_ofs = { 0,0 };
            TimeStamp cpu_time = { 0,0 };
            virtual void setupNIDAQ(std::shared_ptr<Lockable<NIDAQUtils>> nidaq_task,
                                    bool testConfigEnabled, string trial_info,
                                    std::shared_ptr<TestConfig> testConfig,
                                    std::shared_ptr<Lockable<GetTime>> gettime,
                                    unsigned int cameraNumber);
            

        private:

            spinSystem hSystem_ = nullptr;
            spinCamera hCamera_ = nullptr;

            spinNodeMapHandle hNodeMapCamera_ = nullptr;
            spinNodeMapHandle hNodeMapTLDevice_ = nullptr;

            NodeMapCamera_spin nodeMapCamera_;
            NodeMapTLDevice_spin nodeMapTLDevice_;
            NodeMapTLStream_spin nodeMapTLStream_;
            
            CameraInfo_spin cameraInfo_;

            TimeStamp timeStamp_ = {0,0};
            int64_t timeStamp_ns_ = 0;
            int64_t timeStamp_cam = 0;
            int64_t timeStamp_inc = 0;
            int64_t counter=0;
            TimeStamp timeStampDevice_= {0,0};

            bool imageOK_ = false;
            spinImage hSpinImage_ = nullptr;

            bool testConfigEnabled_;
            string trial_num_;           

            TriggerType triggerType_ =  TRIGGER_TYPE_UNSPECIFIED;

            bool grabImageCommon(std::string &errMsg);
            bool releaseSpinImage(spinImage &hImage);
            bool destroySpinImage(spinImage &hImage);

            void setupTimeStamping();
            void updateTimeStamp();
            

            // Get Property Info methods
            static std::map<PropertyType, std::function<PropertyInfo(CameraDevice_spin*)>> getPropertyInfoDispatchMap_; 

            PropertyInfo getPropertyInfoBrightness();
            PropertyInfo getPropertyInfoGamma();
            PropertyInfo getPropertyInfoShutter();
            PropertyInfo getPropertyInfoGain();
            PropertyInfo getPropertyInfoTriggerDelay();
            PropertyInfo getPropertyInfoFrameRate();
            PropertyInfo getPropertyInfoTemperature();
            PropertyInfo getPropertyInfoTriggerMode();

            // Get property methods
            static std::map<PropertyType, std::function<Property(CameraDevice_spin*)>> getPropertyDispatchMap_; 

            Property getPropertyBrightness();
            Property getPropertyGamma();
            Property getPropertyShutter();
            Property getPropertyGain();
            Property getPropertyTriggerDelay();
            Property getPropertyFrameRate();
            Property getPropertyTemperature();
            Property getPropertyTriggerMode();

            // Set Property methods
            bool isPropertySettable(PropertyType propType, std::string &msg);
            static std::map<PropertyType, std::function<void(CameraDevice_spin*,Property)>> setPropertyDispatchMap_; 

            void setPropertyBrightness(Property prop);
            void setPropertyGamma(Property prop);
            void setPropertyShutter(Property prop);
            void setPropertyGain(Property prop);
            void setPropertyTriggerDelay(Property prop);
            void setPropertyFrameRate(Property prop);
            void setPropertyTemperature(Property prop);
            void setPropertyTriggerMode(Property prop);

            PixelFormat getPixelFormat();
            bool isSupportedPixelFormat(PixelFormat pixelFormat, ImageMode imgMode);

            // spin get methods
            // ---------------
            std::vector<spinPixelFormatEnums> getSupportedPixelFormats_spin();
            spinPixelFormatEnums getPixelFormat_spin();

        //test
            std::vector<float> time_stamp1;
            std::vector<float> time_stamp2;
            std::vector<std::vector<uInt32>> time_stamp3;
            std::vector<int64_t> skipFrames;

            std::shared_ptr<Lockable<GetTime>> gettime_;
            std::shared_ptr<Lockable<NIDAQUtils>> nidaq_task_;
            std::shared_ptr<TestConfig>testConfig_;

            int numFrameskip=0;
            unsigned int cameraNumber_;

            void skipDetected(StampedImage& stampedImage);

    };


    typedef std::shared_ptr<CameraDevice_spin> CameraDevicePtr_spin;

}

#endif // #ifndef BIAS_CAMERA_DEVICE_SPIN_HPP
#endif // #ifdef WITH_SPIN
