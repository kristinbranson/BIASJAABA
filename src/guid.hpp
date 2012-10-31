#ifndef GUID_HPP
#define GUID_HPP
#include <string>
#include <iostream>
#include <memory>
#include <stdint.h>
#include "basic_types.hpp"
#include "guid_device_base.hpp"

#ifdef WITH_FC2
#include "C/FlyCapture2_C.h"
#include "guid_device_fc2.hpp"
#endif

#ifdef WITH_DC1394
#include "guid_device_dc1394.hpp"
#endif


namespace bias {
    
    class Guid 
    {
        // --------------------------------------------------------------------
        // Guid -  a Facade wrapper for camera guids. Its purpose is to create
        // a unified interface for the camera guids which are used by different 
        // underlying libraries  e.g. libdc1394, flycapture2, etc.
        // --------------------------------------------------------------------
        
        friend bool operator== (Guid &guid0, Guid &guid1);
        friend bool operator!= (Guid &guid0, Guid &guid1);
        friend std::ostream& operator<< (std::ostream &out, Guid &guid);

        public:
            Guid();
            ~Guid() {};
            CameraLib getCameraLib();
            void printValue();
            std::string toString();

        private:
            std::shared_ptr<GuidDevice_base> guidDevicePtr_;

#ifdef WITH_FC2
        // FlyCapture2 specific features
        public:
            Guid(fc2PGRGuid guid);
            fc2PGRGuid getValue_fc2();
#endif
#ifdef WITH_DC1394
        // Libdc1394 specific features
        public:
            Guid(uint64_t guid);
            uint64_t getValue_dc1394();
#endif
           
    };

}

#endif // #ifndef GUID_HPP
