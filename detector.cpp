using namespace std;

#include "detector.h"
#include "polifitgsl.h"

#include <libconfig.h++>

using namespace cv;

//-----CLASS METHOD DECLARATIONS-----//

int polynomial(const double *params, int degree, double x);

//-----CLASS METHODS-----//

Detector::Detector(string config_path)
{
    libconfig::Config cfg;
    try
    {
        cfg.readFile(config_path.c_str());
        threshold = cfg.lookup("detector.threshold");
        row_step = cfg.lookup("detector.row_step");
        col_step = cfg.lookup("detector.col_step");
        l_start = cfg.lookup("detector.start.left");
        r_start = cfg.lookup("detector.start.right");
    }
    catch(...)
    {
        cerr << "Invalid config file" << endl;
    }
}

/**
 * Get lanes
 * @param img processed (thresholded and warped to birdseye perpective) frame from video
 * @param lane Detected lane
 */
void Detector::getLanes(const Mat &img, Lane &lane)
{    
    int width = img.cols;
    int height = img.rows;
    
    int left = width * l_start / 100;
    int right = width * r_start / 100;
    
    vector<double> lx;
    vector<double> rx;
    vector<double> ly;
    vector<double> ry;
    
    //Loop through frame rows at row_step
    for (int i = height-1; i >= 0; i-=row_step)
    {
        //Loop through left side
        lx.push_back(left);
        ly.push_back(i);
        for (int j = 0; j <= threshold; j+=col_step)
        {
            if (img.at<uchar>(i, left+j) == 255) 
            {
                lx.back() = left+j;
                left += j;
                break;
            }
            
            if (img.at<uchar>(i, left-j) == 255) 
            {
                lx.back() = left-j;
                left -= j;
                break;
            }
        }
        
        //Loop through right side
        rx.push_back(right);
        ry.push_back(i);
        for (int j = 0; j <= threshold; j+=col_step)
        {
            if (img.at<uchar>(i, right-j) == 255) 
            {
                rx.back() = right-j;
                right -= j;
                break;
            }
            
            if (img.at<uchar>(i, right+j) == 255) 
            {
                rx.back() = right+j;
                right += j;
                break;
            }
        }
         
    }
    
    vector<double> l_new(lane.getN(), 0.0);
    vector<double> r_new(lane.getN(), 0.0);
    polynomialfit(lx.size(), lane.getN(), &ly[0], &lx[0], &l_new[0]);
    polynomialfit(rx.size(), lane.getN(), &ry[0], &rx[0], &r_new[0]);
    
    lane.update(l_new, r_new);
}

/**
 * Draws lane on an image
 * @param img Image on which to draw lane
 * @param lane Lane to draw
 * @param m perspective transform matrix for lane
 */
void Detector::drawLane(Mat &img, Lane &lane, Mat &m)
{
    Mat blank(img.size(), img.type(), Scalar(0, 0, 0));
    for (int i = 0; i < img.rows; i++)
    {
        circle(blank, Point(polynomial(&(lane.getParams())[0], lane.getN(), i), i), 3, Scalar(150, 0, 0), 3);
    }
    warpPerspective(blank, blank, m, Size(img.cols, img.rows));
    for (int i = 0; i < img.rows; i+=2)
    {
        for (int j = 0; j < img.cols; j+=2)
        {
            if (blank.at<Vec3b>(i, j)[0] == 150)
                circle(img, Point(j, i), 1, Scalar(150, 0, 0), 1);
        }
    }
}



/**
 * Gets the perpective transform matrix for warping an image to birdseye perspective
 * @param img Image to generate matrix for
 * @param undo If undo is true, return the matrix for transforming from birdseye to first-person perspective
 * @return perspective transform matrix
 */
Mat Detector::getTransformMatrix(Mat img, bool undo)
{
    int width = img.cols;
    int height = img.rows;
    vector<Point2f> src = {Point2f(width*0.44,height*0.20), Point2f(width*0.56,height*0.20), Point2f(width*1.00,height*0.85), Point2f(width*0.00,height*0.85)};
    vector<Point2f> dst = {Point2f(width*0.20,height*0.00), Point2f(width*0.80,height*0.00), Point2f(width*0.80,height*1.00), Point2f(width*0.20,height*1.00)};
    
    Mat m;
    if (undo) m = getPerspectiveTransform(&dst[0], &src[0]);
    else m = getPerspectiveTransform(&src[0], &dst[0]);
    return m;
}

/**
 * Thresholds the image. Uses GPU acceleration
 * Process:
 *   1. Convert image to grayscale
 *   2. Blur the image to remove noise (gaussian)
 *   3. threshold image (binary)
 * @param src image to threshold (GpuMat)
 * @param dst destination for thresholded image (GpuMat)
 */
void Detector::thresh(cv::Mat &src, cv::Mat &dst)
{
    cv::cvtColor(src, dst, CV_BGR2GRAY);
    
    cv::GaussianBlur(dst, dst, Size( 7, 7 ), 1.5, 1.5 );
    cv::threshold(dst, dst, 185, 255, THRESH_BINARY);
}


//-----NON CLASS METHODS-----//


/**
 * Evaluates a polynomial expression
 * @param params Array of polynomial coefficients
 * @param degree Degree of polynomial (size of params)
 * @param x Polynomial input
 * @return Evaluated expression.
 */
int polynomial(const double *params, int degree, double x)
{
    double val = 0;
    for (int i = 0; i < degree; i++)
    {
        val += params[i] * pow(x, i);
    }
    return (int)val;
}	