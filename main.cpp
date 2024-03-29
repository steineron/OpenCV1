#include "opencv2/core/utility.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <stdio.h>


using namespace cv;
using namespace std;


int edgeThresh = 1;
Mat image, gray, blurImage, edge1;


const int w = 500;

/**
 * find the closesest point to @param p in @param points by computing
 * the minimum Euclidean Distance between the point p and any of the other points
 * @param p
 * @param points
 * @return the point in points closest to p
 */
static Point2f closestPoint(Point &p, Point2f points[], int nPoints){

    int minDistance = INT_MAX;
    Point2f result;
    for(int i=0; i<nPoints; i++){
        Point2f candidate = points[i];
        int distance = pow(candidate.x - p.x, 2) + pow(candidate.y - p.y, 2);
        if(distance < minDistance){
            minDistance=distance;
            result=candidate;
        }
    }
    
    return result;
}


static void processImage(Mat &srcImage) {

    // create a BW image:
    cvtColor(srcImage, gray, COLOR_BGR2GRAY);


    // blur it to educe noise
    blur(gray, blurImage, Size(3, 3));

    namedWindow("temp", 1);
    imshow("temp", blurImage);
    waitKey(0);


    Mat binary;
    binary.create(gray.size(), CV_8UC1);
    // convert to binary image
    threshold(blurImage, binary, 25, 255, THRESH_BINARY_INV + THRESH_OTSU);

    imshow("temp", binary);
    waitKey(0);

    if (binary.type() != CV_8UC1) {
        binary.convertTo(binary, CV_8UC1);
        imshow("temp", binary);
        waitKey(0);

    }
    // dilate, erode, dilate to further remove noise and small objects
    int niters = 3; // in 3 iterations

    dilate(binary, binary, Mat(), Point(-1, -1), niters);
    erode(binary, binary, Mat(), Point(-1, -1), niters * 2);
    dilate(binary, binary, Mat(), Point(-1, -1), niters);

    // Run the edge detector on grayscale
    Canny(binary, edge1, edgeThresh, edgeThresh * 3, 3);

    imshow("temp", edge1);
    waitKey(0);


    int levels = 3;
    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    vector<vector<Point> > contours0;
    findContours(edge1, contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
    contours.resize(contours0.size());
    for (size_t k = 0; k < contours0.size(); k++)
        approxPolyDP(Mat(contours0[k]), contours[k], 3, true);

    // see example in https://docs.opencv.org/master/db/d00/samples_2cpp_2squares_8cpp-example.html#a17
    // for extracting squares

    // or do this: find the contour with largest area - assume it is the document
    Mat contouredImage = Mat::zeros(w, w, CV_8UC3);
    srcImage.copyTo(contouredImage);
    int _levels = levels - 3;
    int maxArea = 0, max = 0;
    vector<Point> approxPoly;
    for (int i = 0; i < contours.size(); i++) {
        int current = contourArea(contours[i], false);
        if (current > max) {
            // check the shape of it:
            vector<Point> approx;
            approxPolyDP(contours[i], approx, arcLength(contours[i], true) * 0.02, true);

            // square contours should have 4 vertices after approximation
            // relatively large area (to filter out noisy contours)
            // and be convex.
            // Note: absolute value of an area is used because
            if (approx.size() == 4 && isContourConvex(approx)) {
                max = current;
                maxArea = i;
                approxPoly = approx; // keep record of hte approximated polygon
            }
        }
    }

    drawContours(contouredImage, contours, maxArea, Scalar(128, 255, 255),
                 3, LINE_AA, hierarchy, std::abs(_levels));


    RotatedRect rect = minAreaRect(contours[maxArea]);
    Rect boundRect = boundingRect(contours[maxArea]);

    Point2f box[4];
    rect.points(box);
    Scalar blue = Scalar(255, 128, 0);
    Scalar red = Scalar(0, 128, 255);
    Scalar black = Scalar(0, 0, 0);
    Scalar orange = Scalar(0,255,220);

    String numbers[] = {"1", "2", "3", "4"};

    double textScale = 5;
    int textThinkness = 4;

    for (int j = 0; j < 4; j++) {
        line(contouredImage, approxPoly[j], approxPoly[(j + 1) % 4], blue, 3, LINE_AA);
        putText(contouredImage, numbers[j], approxPoly[j], FONT_HERSHEY_SIMPLEX,textScale,blue,textThinkness);
        line(contouredImage, box[j], box[(j + 1) % 4], red, 3, LINE_AA);
        putText(contouredImage, numbers[j], box[j], FONT_HERSHEY_SIMPLEX,textScale,red,textThinkness);
    }
    // draw the bounding rect
    rectangle(contouredImage, Point(boundRect.x, boundRect.y),
              Point(boundRect.x + boundRect.width, boundRect.y + boundRect.height), black, 3, LINE_AA);



    // transform the skewed image
    std::vector<Point2f> polyPoints;
    std::vector<Point2f> boxPoints;
    std::vector<Point2f> boundingRectPoints;
    // poly to box:

    for(int i=0; i<4; i++){
        polyPoints.push_back(Point2f(approxPoly[i].x, approxPoly[i].y));
        boundingRectPoints.push_back(closestPoint(approxPoly[i], box,4));
    }

    for (int j = 0; j < 4; j++) {
        putText(contouredImage, numbers[j], boundingRectPoints[j], FONT_HERSHEY_SIMPLEX,textScale,black,textThinkness,LINE_8);
        line(contouredImage, boundingRectPoints[j], polyPoints[j], orange, 3, LINE_AA);
    }

    namedWindow("contours", 1);
    imshow("contours", contouredImage);

    Mat transmtx = getPerspectiveTransform(polyPoints, boundingRectPoints);
    Mat transformed = Mat::zeros(srcImage.rows, srcImage.cols, CV_8UC3);
    warpPerspective(srcImage, transformed, transmtx, srcImage.size());

    // box to rect:

    polyPoints.clear();
    boundingRectPoints.clear();


    float h = MIN(rect.size.height,rect.size.width);
    float w = MAX(rect.size.height,rect.size.width);
    boundingRectPoints.push_back(Point2f(boundRect.x, boundRect.y));
    boundingRectPoints.push_back(Point2f(boundRect.x, boundRect.y + h));
    boundingRectPoints.push_back(Point2f(boundRect.x + w, boundRect.y));
    boundingRectPoints.push_back(Point2f(boundRect.x + w, boundRect.y + h));

    for (int i = 0; i < 4; ++i) {
        Point p = Point(boundingRectPoints[i].x, boundingRectPoints[i].y);
        polyPoints.push_back(closestPoint(p, box, 4));

    }


    contouredImage = image.clone();

    for (int j = 0; j < 4; j++) {
        line(contouredImage, polyPoints[j], polyPoints[(j + 1) % 4], red, 3, LINE_AA);
        putText(contouredImage, numbers[j], polyPoints[j], FONT_HERSHEY_SIMPLEX,textScale,red,textThinkness);
        line(contouredImage, boundingRectPoints[j], boundingRectPoints[(j + 1) % 4], orange, 3, LINE_AA);
        putText(contouredImage, numbers[j], boundingRectPoints[j], FONT_HERSHEY_SIMPLEX,textScale,orange,textThinkness);
        line(contouredImage, boundingRectPoints[j], polyPoints[j], blue, 3, LINE_AA);
    }

    namedWindow("contours-transformed", 1);
    imshow("contours-transformed", contouredImage);

    transmtx = getPerspectiveTransform(polyPoints, boundingRectPoints);
    warpPerspective(transformed, transformed, transmtx, transformed.size());

    namedWindow("transformed", 1);
    imshow("transformed", transformed);


}

static void help() {
    printf("\nThis sample demonstrates Canny edge detection\n"
           "Call:\n"
           "    /.OpenCv1 <path_to_image_file>\n\n");
}

const char *keys =
        {
                "{help h||}{@image |some_image.jpg|input image name}"
        };

int main(int argc, const char **argv) {
    help();
    CommandLineParser parser(argc, argv, keys);
    string filename = parser.get<string>(0);
    image = imread(samples::findFile(filename), IMREAD_COLOR);
    if (image.empty()) {
        printf("Cannot read image file: %s\n", filename.c_str());
        help();
        return -1;
    }
    processImage(image);
    // Wait for a key stroke; the same function arranges events processing
    waitKey(0);
    return 0;
}