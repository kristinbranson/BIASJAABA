cmake_minimum_required(VERSION 2.8 FATAL_ERROR) 
project(bais)

if (WIN32)
    set (CMAKE_SYSTEM_VERSION 10 CACHE TYPE INTERNAL FORCE)
endif()

if (POLICY CMP0020) 
    cmake_policy(SET CMP0020 NEW)
endif()
cmake_policy(SET CMP0046 OLD)


# Options
# -----------------------------------------------------------------------------
option(with_qt_gui  "include image acquisition GUI"   ON) 
option(with_fc2     "include the FlyCapture2 backend" OFF)
option(with_dc1394  "include the libdc1394 backend"   OFF)
option(with_spin    "include the spin backend"   ON)
option(with_demos   "include demos" OFF)
option(with_tests   "include tests" OFF)

message(STATUS "Option: with_fc2     = ${with_fc2}")
message(STATUS "Option: with_dc1394  = ${with_dc1394}")
message(STATUS "Option: with_qt_gui  = ${with_qt_gui}")
message(STATUS "Option: with_demos   = ${with_demos}")
message(STATUS "Option: with_tests   = ${with_tests}") 

if( NOT( with_fc2 OR with_dc1394 OR with_spin) )
    message(FATAL_ERROR "their must be at least one camera backend")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

# Opencv library
# -----------------------------------------------------------------------------
find_package( OpenCV 4 REQUIRED )

if(${OpenCV_FOUND})
    message(STATUS "OpenCV found")
else()
    message(STATUS "OpenCv not found")
endif()


# FlyCapture2 library location - replace this this custom find_package
# -----------------------------------------------------------------------------
if(with_fc2)
    find_package( FlyCapture2 MODULE REQUIRED )
endif()

if(with_spin)
    find_package( Spinnaker MODULE REQUIRED )
endif()

# Qt library
# ------------------------------------------------------------------------------
if(with_qt_gui)
    set(QT_BINARY_DIR "$ENV{QTDIR}/bin")

    # Old qt 4.8 build
    # ------------------------------------------------------------
    #find_package(Qt4 COMPONENTS QtCore QtGui QtNetwork REQUIRED)
    # ------------------------------------------------------------

    find_package(Qt5Core REQUIRED) 
    find_package(Qt5Widgets REQUIRED)
    find_package(Qt5Gui REQUIRED) 
    find_package(Qt5Network REQUIRED)

endif()

set(HDF5_USE_STATIC_LIBRARIES ON)
find_package(HDF5 COMPONENTS CXX)
if(${HDF5_FOUND})
    message(STATUS "HDF5 Found")
else()
    message(STATUS "HDF5 Not Found")
endif()

# Compiler flags
# -----------------------------------------------------------------------------
#set(CMAKE_CXX_FLAGS "-std=gnu++0x -O2 -Wall")
#set(CMAKE_CXX_FLAGS "-std=gnu++0x -O2")
#set(CMAKE_CXX_FLAGS "-std=gnu++0x -mwindows -O3")
#set(CMAKE_CXX_FLAGS "-std=c++11 ")

#CUDA_SELECT_NVCC_ARCH_FLAGS(ARCH_FLAGS 6.1)
#list(APPEND CUDA_NVCC_FLAGS ${ARCH_FLAGS})
set(CMAKE_CUDA_ARCH 3.5 5 6 7.5) 
set(CMAKE_CUDA_CODE 3.5 5 6 7.5)

message(STATUS "${CMAKE_CUDA_ARCH}")

set(CUDA_NVCC_FLAGS_DEBUG          -Xcompiler -fPIC;-std=c++11;-g;-G CACHE STRING "" FORCE)
set(CUDA_NVCC_FLAGS_RELEASE        -Xcompiler -fPIC;-u-std=c++11;--use_fast_math;-g;-lineinfo CACHE STRING "" FORCE)
set(CUDA_NVCC_FLAGS_MINSIZEREL     -Xcompiler -fPIC;--std=c++11;-use_fast_math CACHE STRING "" FORCE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD   99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE RELEASE)

# Definitions 
# -----------------------------------------------------------------------------
 add_definitions(-DDebug)

if(with_fc2)
    add_definitions(-DWITH_FC2)
endif()

if(with_spin)
    add_definitions(-DWITH_SPIN)
endif()

if(with_dc1394)
    add_definitions(-DWITH_DC1394)
endif()


# Include directories
# -----------------------------------------------------------------------------

include_directories("./src/backend/base")
include_directories("./src/facade")
include_directories("./src/utility")
include_directories("./src/gui")
include_directories("./src/plugin/base")
include_directories("./src/plugin/stampede")
include_directories("./src/plugin/grab_detector")
include_directories("./src/3rd_party/qcustomplot")


if(UNIX)
    set(HEFFALUMP_SRC_DIR /groups/branson/home/patilr/rutuja_heffalump/heffalump)
    include_directories(${HEFFALUMP_SRC_DIR}/src/gpu)
                        #${HEFFALUMP_SRC_DIR}/src)
endif()

if(WIN32)
    set(HEFFALUMP_SRC_DIR C:/Users/patilr/rutuja_heffalump/heffalump)
    include_directories(${HEFFALUMP_SRC_DIR}/src/gpu)
                        #${HEFFALUMP_SRC_DIR}/src)
endif()

if(with_demos) 
    include_directories(${CUDA_INCLUDE_DIRS} 
                        ${HDF5_INCLUDE_DIRS})
endif()

if(with_fc2)
    include_directories(${FlyCapture2_INCLUDE_DIRS})
    include_directories("./src/backend/fc2")
endif()

if (with_spin)
    include_directories(${Spinnaker_INCLUDE_DIRS})
    include_directories("./src/backend/spin")
endif()

if(with_dc1394)
    include_directories("./src/backend/dc1394")
    # Add custom libdc1394 
    #include_directories("/home/wbd/local/include")
    #link_directories("/home/wbd/local/lib")
endif()


# External link libraries
# -----------------------------------------------------------------------------
set(bias_ext_link_LIBS ${OpenCV_LIBS})
message(STATUS "${OPENCV_LIBS}")


if(with_fc2)
    set(bias_ext_link_LIBS ${bias_ext_link_LIBS} ${FlyCapture2_LIBRARIES})
endif()

if (with_spin)
    set(bias_ext_link_LIBS ${bias_ext_link_LIBS} ${Spinnaker_LIBRARIES})
endif()

if(with_dc1394)
    if (WIN32)
        set(bias_ext_link_LIBS ${bias_ext_link_LIBS} dc1394 1394camera setupapi)
    else()
        set(bias_ext_link_LIBS ${bias_ext_link_LIBS} dc1394)
    endif()
endif()
set(bias_ext_link_LIBS ${bias_ext_link_LIBS})
message(STATUS "${bias_ext_link_LIBS}")


# Output directory for executables - puts executables in build directory
# -----------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Add project subdirectories
# -----------------------------------------------------------------------------
add_subdirectory("src/backend/base") 

if(with_fc2) 
    add_subdirectory("src/backend/fc2") 
endif()

if(with_dc1394) 
    add_subdirectory("src/backend/dc1394")
endif() 

if(with_spin)
    add_subdirectory("src/backend/spin")
endif()

add_subdirectory("src/facade")
add_subdirectory("src/utility")
add_subdirectory("src/plugin/base")
add_subdirectory("src/plugin/stampede")
add_subdirectory("src/plugin/grab_detector")
add_subdirectory("src/3rd_party/qcustomplot")

if(with_qt_gui)
    add_subdirectory("src/gui")
endif()

if (with_demos)
    add_subdirectory("src/demo/fly_sorter")
endif()

if (with_tests)
    add_subdirectory("src/test")
endif()

