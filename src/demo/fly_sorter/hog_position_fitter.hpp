#ifndef HOG_POSITION_FITTER
#define HOG_POSITION_FITTER
#include "parameters.hpp"
#include "fly_segmenter.hpp"
#include <list>
#include <opencv2/core/core.hpp>

class PositionData
{
    public:

        bool success;
        bool isFly;
        bool isMultipleFlies;
        unsigned int bodyArea;
        double meanX;
        double meanY;
        double ellipseMajorAxis;
        double ellipseMinorAxis;
        double ellipseAngle;
        cv::Mat covarianceMatrix;

        PositionData();

        static const bool DEFAULT_IS_FLY;
        static const bool DEFAULT_IS_MULTIPLE_FLIES;
};
typedef std::list<PositionData> PositionDataList;


class HogPositionFitterData
{
    public:
        PositionDataList positionDataList;
        HogPositionFitterData();
};


class HogPositionFitter
{
    public:

        HogPositionFitter();
        HogPositionFitter(HogPositionFitterParam param);
        void setParam(HogPositionFitterParam param);
        HogPositionFitterData fit(FlySegmenterData flySegmenterData);

    private:
        bool showDebugWindow_;
        HogPositionFitterParam param_;
        cv::Mat getPixelFeatureVector(cv::Mat image);

};


enum GradientMethod
{
    GRAD_METHOD_SOBEL,
    GRAD_METHOD_SCHARR,
};


class GradientData
{
    public:
        cv::Mat dx;              // derivative w.r.t. x
        cv::Mat dy;              // derivative w.r.t. y
        cv::Mat mag;             // gradient magnitude
        cv::Mat ori;             // gradient orientation
        cv::Mat normMag;         // normalized gradient magnitude
        cv::Mat normMagMax;      // maximum (over all channels) of normalized gradient magnitude
        cv::Mat oriOfNormMagMax; // orientation of maximum gradient
};


GradientData getGradientData(
        cv::Mat image, 
        unsigned int normRadius, 
        double normConst,
        GradientMethod method
        );

cv::Mat getTriangleFilter1D(unsigned int normRadius);
cv::Mat getTriangleFilter2D(unsigned int normRadius);


#endif
