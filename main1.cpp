// Program to detect simple grids out of an image
// By Mekaribity
#include <stdio.h>
#include <ctype.h>
#include <iostream>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;

typedef struct
{
	Point2i start, end;
} Line;

void convertToGrayScaleAndEdgeDetect(Mat &inputImage, Mat &outputImage);
vector<Line> getLines(Mat &inputImage, unsigned int skipPixels = 0, unsigned int minLinePixels = 3, void(*debug)(Mat output) = NULL);
void resetLine(Line& line);
void drawLines(vector<Line> lines, Mat& outputImage);
void debugGridLines(Mat& image);
void debugRecursive(Mat& image);

int main(int argc, char** argv)
{

	char* imageFileName = "input2.bmp";
	char* outputFileName = "output.bmp";

	cv::Mat inputImage = cv::imread(imageFileName);
	cv::Mat outputImage = cv::Mat::zeros(inputImage.size(), inputImage.type());
	cv::Mat grayImage = cv::Mat::zeros(inputImage.size(), inputImage.type());
	cv::Mat linesImage = cv::Mat::zeros(inputImage.size(), inputImage.type());;

	convertToGrayScaleAndEdgeDetect(inputImage, grayImage);

	// vector<Line> lines = getLines(grayImage, 0, 10);
	// drawLines(lines, linesImage);
	//debugGridLines(grayImage);
	debugRecursive(grayImage);

	// preview images
	
	cv::namedWindow("Original Image", 1);
	cv::namedWindow("Lines Image", 1);
	cv::namedWindow("Filtered Image", 1);

	cv::imshow("Original Image", inputImage);
	cv::imshow("Lines Image", linesImage);
	cv::imshow("Filtered Image", grayImage);

	cv::waitKey();
	
	return 0;
}

// TODO: Menu bar filter, identifies dashes between 2 blank lines and erases them
// TODO: add find intersections for given lines
// TODO: Grid line extension algorithm based on bounding box and averaging
// TODO: thickness removal
// debug recursive line detection
void debugRecursive(Mat& image)
{
	cv::Mat linesImage(image);
	char buf[128];
	int skip = 0;
	VideoWriter video("recursive.avi", CV_FOURCC('M', 'P', '4', '2'), 1, Size(image.cols, image.rows), false);
	for (int i = 3; i < 30; i+=5)
	{
		printf("preparing frame for minlinepx: %d, skippx: %d\n", i, skip);
		vector<Line> lines = getLines(linesImage, skip, i);
		linesImage = cv::Mat::zeros(image.size(), image.type());
		drawLines(lines, linesImage);
		sprintf_s(buf, "min: %d, skip: %d", i, skip);
		putText(linesImage, buf, Point(10, 10), FONT_HERSHEY_PLAIN, 1.0, Scalar(255, 255, 255));
		sprintf_s(buf, "min%dskip%d.bmp", i, skip);
		imwrite(buf, linesImage);
		video.write(linesImage);

		/*if (skip++ > 7)
			break;*/
	}
	video.release();
}

void debugGridLines(Mat& image)
{
	cv::Mat linesImage = cv::Mat::zeros(image.size(), image.type());;
	char buf[128];
	VideoWriter video("lines.avi", CV_FOURCC('M', 'P', '4', '2'), 2, Size(image.cols, image.rows), false);
	for (int i = 1; i < 30; i++)
	{
		for (int j = 0; j < 5; j++)
		{
			printf("preparing frame for minlinepx: %d, skippx: %d\n", i, j);
			vector<Line> lines = getLines(image, j, i);
			linesImage = cv::Mat::zeros(image.size(), image.type());
			drawLines(lines, linesImage);
			sprintf_s(buf, "min: %d, skip: %d", i, j);
			putText(linesImage, buf, Point(10, 10), FONT_HERSHEY_PLAIN, 1.0, Scalar(255, 255, 255));
			video.write(linesImage);
		}
	}
	video.release();
}

void convertToGrayScaleAndEdgeDetect(Mat& inputImage, Mat& outputImage)
{
	int ddepth = CV_8U;
	int scale = 1;
	int delta = 0;
	int kernel_size = 1;

	cv::Mat grayImage = cv::Mat::zeros(inputImage.size(), inputImage.type());

	cvtColor(inputImage, grayImage, CV_BGR2GRAY);
	Laplacian(grayImage, outputImage, ddepth, kernel_size, scale, delta, BORDER_DEFAULT);
}

void resetLine(Line& line)
{
	line.start.x = 0;
	line.start.y = 0;
	line.end.x = 0;
	line.end.y = 0;
}

// FIXME: sometimes, you'll find diagonal lines, starting from 0,0 to some 3,344
// FIXME: since the skippixels is set, every pixel is considered a line and a line is made with the trailing skipped pixels
// creating a distorted image
vector<Line> getLines(Mat &inputImage, unsigned int skipPixels, unsigned int minLinePixels, void (*debug)(Mat output))
{
	vector<Line> lines;
	// how much continous pixels make a line
	// NOTE: 0 is not an acceptable line
	// its equivalent to allowing a pixel to be set
	//int minLinePixels = 3;
	// how many continous pixels can we skip
	//int skipPixels = 0;
	int pixelThreshold = 20;
	int width = inputImage.cols;
	int height = inputImage.rows;
	Mat output = cv::Mat::zeros(inputImage.size(), inputImage.type());
	// Mat dbgIm = cv::Mat::zeros(inputImage.size(), inputImage.type());
	bool newLine = false, newLineStarted = false;
	unsigned int skippedPixels = 0;
	// 0 - horizontal, 1 - vertical
	int direction = 0;

	Line currLine = 
	{
		Point(0, 0), Point(0, 0)
	};

	do
	{
		for (int y = 0; y < (direction == 0 ? height : width); y++)
		{
			newLine = true;
			for (int x = 0; x < (direction == 0 ? width : height); x++)
			{
				int pixel = direction == 1 ? (unsigned char)inputImage.at<unsigned char>(x, y)
					: (unsigned char)inputImage.at<unsigned char>(y, x);

				// if its a new line, 
				// then reset the line coords to zero
				if (newLine)
				{
					resetLine(currLine);
					newLine = false;
				}

				// if its a bright pixel then
				// consider a line has started
				if (pixel > pixelThreshold)
				{
					//if (direction == 0)
					//	dbgIm.at<unsigned char>(y, x) = 0xff;
					// set the state that
					// we are drawing a new line
					if (!newLineStarted)
					{
						newLineStarted = true;
						currLine.start.x = direction == 0 ? x : y;
						currLine.start.y = direction == 0 ? y : x;
					}
					skippedPixels = 0;
				}
				else
				{
					// count continuos blank pixels
					++skippedPixels;
				}

				if (newLineStarted)
				{
					// keep setting the end points until we see blank pixels
					currLine.end.x = direction == 0 ? x : y;
					currLine.end.y = direction == 0 ? y : x;
				}

				// if continous blank pixels are found and a line
				// is found, then check further and add a line
				if (newLineStarted && skippedPixels > skipPixels)
				{
					// reject lines less than minLine pixels
					if ((unsigned int)(direction == 0 ? currLine.end.x - currLine.start.x
						: currLine.end.y - currLine.start.y) >= minLinePixels)
					{
						// trim the trailing blank pixels of line tail
						if (direction == 0)
							currLine.end.x -= skipPixels;
						else
							currLine.end.y -= skipPixels;
						

						// verbose line output
						//printf("line, sx: %d, sy: %d, ex: %d, ey: %d\n", currLine.start.x, currLine.start.y, currLine.end.x, currLine.end.y);
						// detect diagnonals
						if (direction == 1 ? currLine.start.x == currLine.end.x : currLine.start.y = currLine.end.y)
						{
							// end the line
							// madeline
							lines.push_back(currLine);
						}
						else
						{
							// printf("diagon-alley, sx: %d, sy: %d, ex: %d, ey: %d\n", currLine.start.x, currLine.start.y, currLine.end.x, currLine.end.y);
						}
						
					}
					// reset states
					skippedPixels = 0;
					newLineStarted = false;
					currLine.start.x = direction == 0 ? x : y;
					currLine.start.y = direction == 0 ? y : x;
					currLine.end.x = direction == 0 ? x : y;
					currLine.end.y = direction == 0 ? y : x;
				}
			}
		}
	} while (++direction < 2);

	if (debug != NULL)
	{
		debug(output);
	}

	// cv::namedWindow("Debug Image", 1);
	// cv::imshow("Debug Image", dbgIm);

	return lines;
}

void drawLines(vector<Line> lines, Mat& outputImage)
{
	for (unsigned int i = 0; i < lines.size(); i++)
	{
		Line line = lines.at(i);
		cv::line(outputImage, line.start, line.end, Scalar(0xff, 0xff, 0xff));
	}
}
