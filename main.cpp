// Program to detect simple grids out of an image
#include <stdio.h>
#include <ctype.h>
#include <iostream>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

int main(int argc, char** argv)
{

	// read image file
	std::string imageFileName = "input.bmp";
	std::string outputFileName = "output.bmp";

	cv::Mat inputImage = cv::imread(imageFileName);
	cv::Mat outputImage = cv::Mat::zeros(inputImage.size(), inputImage.type());




	cv::namedWindow("Original Image", 1);
	cv::namedWindow("Filtered Image", 1);

	cv::imshow("Original Image", inputImage);
	cv::imshow("Filtered Image", outputImage);

	char key = getchar();
	return 0;
}