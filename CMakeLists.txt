cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(bais)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")


# Options
# -----------------------------------------------------------------------------
option(with_qt_gui  "include image acquisition GUI"   ON ) 
option(with_fc2     "include the FlyCapture2 backend" ON )
option(with_dc1394  "include the libdc1394 backend"   OFF)

message(STATUS "Option: with_fc2     = ${with_fc2}")
message(STATUS "Option: with_dc1394  = ${with_dc1394}")
message(STATUS "Option: with_qt_gui  = ${with_qt_gui}")

if( NOT( with_fc2 OR with_dc1394 ) )
    message(FATAL_ERROR "their must be at least one camera backend")
endif()


# Opencv library
# -----------------------------------------------------------------------------
find_package( OpenCV REQUIRED )

if(${OpenCV_FOUND})
    message(STATUS "OpenCV found")
else()
    message(STATUS "OpenCv not found")
endif()


# FlyCapture2 library location - replace this this custom find_package
# -----------------------------------------------------------------------------
if(with_fc2)
    find_package( FlyCapture2 REQUIRED )
endif()

# Qt library
# ------------------------------------------------------------------------------
if(with_qt_gui)
    set(QT_BINARY_DIR "C:/users/wbd/work/qt/qt_activestate/bin")
    find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
endif()

# Compiler flags
# -----------------------------------------------------------------------------
set(CMAKE_CXX_FLAGS "-std=gnu++0x")


# Definitions 
# -----------------------------------------------------------------------------
add_definitions(-DDebug)

if(with_fc2)
    add_definitions(-DWITH_FC2)
endif()

if(with_dc1394)
    add_definitions(-DWITH_DC1394)
endif()


# Include directories
# -----------------------------------------------------------------------------
include_directories("./src/backend/base")
include_directories("./src/facade")
if(with_fc2)
    include_directories(${FlyCapture2_INCLUDE_DIRS})
    include_directories("./src/backend/fc2")
endif()

if(with_dc1394)
    include_directories("./src/backend/dc1394")
endif()


# External link libraries
# -----------------------------------------------------------------------------
set(bias_ext_link_LIBS ${OpenCV_LIBS})

if(with_fc2)
    set(bias_ext_link_LIBS ${bias_ext_link_LIBS} ${FlyCapture2_LIBRARIES})
endif()

if(with_dc1394)
    set(bias_ext_link_LIBS ${bias_ext_link_LIBS} dc1394)
endif()


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

add_subdirectory("src/facade")
add_subdirectory("src/test")

if(with_qt_gui)
    add_subdirectory("src/gui")
endif()
