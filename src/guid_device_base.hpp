#ifndef GUID_DEVICE_BASE_HPP
#define GUID_DEVICE_BASE_HPP

#include <string>
#include <iostream>
#include "basic_types.hpp"

namespace bias {

    class GuidDevice_base 
    {
        friend bool operator== (GuidDevice_base &guid0, GuidDevice_base &guid1)
        {
            return guid0.isEqual(guid1);
        };
        
        friend bool operator!= (GuidDevice_base &guid0, GuidDevice_base &guid1)
        {
            return !(guid0.isEqual(guid1));
        };
        
        public:
            GuidDevice_base() {};
            virtual ~GuidDevice_base() {};
            virtual CameraLib getCameraLib() { return CAMERA_LIB_UNDEFINED; };
            virtual void printValue() { std::cout << toString() << std::endl; };
            virtual std::string toString() { return std::string("guid value not defined"); };

        private:
            virtual bool isEqual(GuidDevice_base &guid) 
            { 
                return false; 
            };
    }; // class GuidDevice_base

} // namespace bias


#endif // #ifndef GUID_DEVICE_BASE_HPP
