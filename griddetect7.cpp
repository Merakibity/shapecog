// Program to detect simple grids out of an image
// By Mekaribity
#include <stdio.h>
#include <ctype.h>
#include <iostream>
#include <map>
#include <set>
#include <iterator>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#define GETHORVERPOS(direction, line) direction == 0 ? line.start.y : line.start.x
#define GETLINEEND(direction, line) direction == 0 ? line.end.x : line.end.y

#define DIR_HOR 0
#define DIR_VER 1
#define DIR_DIAG 1

using namespace cv;

////////////////////////////////////////////////////////

namespace gridcog
{
	typedef unsigned int uint;
	typedef struct _Line
	{
		Point2i start, end;
	} Line;

	typedef struct _LineMap
	{
		std::map<int, vector<Line>> hLines, vLines;
		std::vector<unsigned int> hLinesPos, vLinesPos;
	}LineMap;

	typedef struct _PointMap
	{
		std::map<int, vector<Point>> hPoints, vPoints;
	}PointMap;

	typedef struct _Rectangle
	{
		int x, y, w, h;
	}Rectangle;

	void convertToGrayScaleAndEdgeDetect(Mat &inputImage, Mat &outputImage);
	vector<Line> getLines(Mat &inputImage, unsigned int skipPixels = 0, unsigned int minLinePixels = 3, void(*debug)(Mat output) = NULL);
	void resetLine(Line& line);
	void drawLines(vector<Line> lines, Mat& outputImage);
	void debugGridLines(Mat& image);
	void debugRecursive(Mat& image);
	vector<Line> removeThickness2(vector<Line>& lines);

	void sortByLineStart(vector<Line>& lines, int direction)
	{
		for (unsigned int i = 0; i < lines.size() - 1; i++)
		{
			for (unsigned int j = i + 1; j < lines.size(); j++)
			{
				if (direction == DIR_HOR ? lines[i].start.x > lines[j].start.x : lines[i].start.y > lines[j].start.y)
				{
					Line tmp = lines[i];
					lines[j] = lines[i];
					lines[i] = tmp;
				}
			}
		}
		// printf("sorted");
		return;
	}

	int inline getMaxLen(int i, Line sandwich[])
	{
		int maxlen = 0;

		// check if the maxlen is eleigible for 
		// the current line

		if (sandwich[i].start.y == sandwich[i].end.y)
		{
			maxlen = sandwich[i].end.x - sandwich[i].start.x;
		}
		else if (sandwich[i].start.x == sandwich[i].end.x)
		{
			maxlen = sandwich[i].end.y - sandwich[i].start.y;
		}
		return maxlen;
	}

	int inline isDirectionChanged(int dir, Line line)
	{
		return (dir == 0 && line.start.x == line.end.x);
	}

	int inline greaterLine(int i, int j, Line sandwich[])
	{
		int len1 = getMaxLen(i, sandwich);
		int len2 = getMaxLen(j, sandwich);

		return len1 >= len2 ? 0 : 1;
	}

	LineMap getLineMap(vector<Line>& lines)
	{
		LineMap lineMap;

		for (unsigned int i = 0; i < lines.size(); i++)
		{
			Line line = lines[i];
			if (!isDirectionChanged(0, line))
			{
				if (lineMap.hLines.find(line.start.y) == lineMap.hLines.end())
					lineMap.hLinesPos.push_back(GETHORVERPOS(DIR_HOR, line));
				lineMap.hLines[line.start.y].push_back(line);
			}
			else
			{
				if (lineMap.vLines.find(line.start.x) == lineMap.vLines.end())
					lineMap.vLinesPos.push_back(GETHORVERPOS(DIR_VER, line));
				lineMap.vLines[line.start.x].push_back(line);
			}
		}

		return lineMap;
	}

	// TODO: Menu bar filter, identifies dashes between 2 blank lines and erases them
	// TODO: add find intersections for given lines
	// TODO: Grid line extension algorithm based on bounding box and averaging

	void swapLinesIfFirstIsLittleX(Line* sandwich)
	{
		if (sandwich[0].start.x > sandwich[1].start.x
			&& sandwich[0].start.x > sandwich[1].end.x
			&& sandwich[0].end.x > sandwich[1].start.x
			&& sandwich[0].end.x > sandwich[1].end.x)
		{
			Line t = sandwich[0];
			sandwich[1] = sandwich[0];
			sandwich[0] = t;
		}
	}

	void swapLinesIfFirstIsLittleY(Line* sandwich)
	{
		if (sandwich[0].start.y > sandwich[1].start.y
			&& sandwich[0].start.y > sandwich[1].end.y
			&& sandwich[0].end.y > sandwich[1].start.y
			&& sandwich[0].end.y > sandwich[1].end.y)
		{
			Line t = sandwich[0];
			sandwich[1] = sandwich[0];
			sandwich[0] = t;
		}
	}

	int inline isThick(Line sandwich[])
	{
		// check for direction
		// and return the results
		// (sandwich[1].start.y == sandwich[0].start.y + 1)
		// || (sandwich[1].start.x == sandwich[0].start.x + 1)
		if (sandwich[0].start.y == sandwich[0].end.y
			&& sandwich[1].start.y == sandwich[1].end.y)
		{
			swapLinesIfFirstIsLittleX(sandwich);
			return (sandwich[1].start.y == sandwich[0].start.y + 1)
				&& ((sandwich[1].end.x <= sandwich[0].end.x && sandwich[1].end.x >= sandwich[0].start.x)
				|| (sandwich[1].start.x <= sandwich[0].end.x && sandwich[1].start.x >= sandwich[0].start.x));
		}
		else if (sandwich[0].start.x == sandwich[0].end.x
			&& sandwich[1].start.x == sandwich[1].end.x)
		{
			swapLinesIfFirstIsLittleY(sandwich);
			return (sandwich[1].start.x == sandwich[0].start.x + 1)
				&& ((sandwich[1].end.y <= sandwich[0].end.y && sandwich[1].end.y >= sandwich[0].start.y)
				|| (sandwich[1].start.y <= sandwich[0].end.y && sandwich[1].start.y >= sandwich[0].start.y));
		}
		return 0;
	}

	void printLine(Line& line)
	{
		printf("sx: %d, sy: %d, ex: %d, ey: %d\n",
			line.start.x, line.start.y, line.end.x, line.end.y);
	}

	void debugSandwich(Line sandwich[])
	{
	}

	void getDiagEndPoint(Point ptX, Point ptY, PointMap& pointMap, Point& ptOut)
	{
		vector<Point> pointsAlongX = pointMap.hPoints[ptY.y];
		vector<Point> pointsAlongY = pointMap.vPoints[ptX.x];
		int found = 0;
		Point ptFound1, ptFound2;

		for (uint h = 0; h < 2; h++)
		{
			for (uint i = 0; i < (h == 0 ? pointsAlongX.size() : pointsAlongY.size()); i++)
			{
				if (h == 0)
				{
					if (pointsAlongX[i].y == ptY.y && pointsAlongX[i].x == ptX.x)
					{
						ptFound1 = pointsAlongX[i];
						found++;
						break;
					}
				}
				else
				{
					if (pointsAlongY[i].x == ptX.x && pointsAlongY[i].y == ptY.y)
					{
						ptFound2 = pointsAlongY[i];
						found++;
						break;
					}
				}
			}
		}

		if (found == 2 && ptFound2.x == ptFound1.x && ptFound1.y == ptFound2.y)
		{
			printf("found: %d, %d\n", ptFound1.x, ptFound1.y);
			ptOut = ptFound1;
		}
	}

	int getNearestPoints(Point pt, PointMap& pointMap, int minX, int minY, Point& ptX, Point& ptY, int& startedX, int& startedY, int startX = 0, int startY = 0)
	{
		vector<Point> pointsAlongX = pointMap.hPoints[pt.y];
		vector<Point> pointsAlongY = pointMap.vPoints[pt.x];
		int found = 0;

		for (uint h = 0; h < 2; h++)
		{
			for (uint i = 0; i < (h == 0 ? pointsAlongX.size() : pointsAlongY.size()); i++)
			{
				if (h == 0)
				{
					startedX = pointsAlongX[i].x;
					if (pointsAlongX[i].x > pt.x && pointsAlongX[i].x > minX)
					{
						ptX = pointsAlongX[i];
						found++;
						break;
					}
				}
				else
				{
					startedY = pointsAlongY[i].y;
					if (pointsAlongY[i].y > pt.y && pointsAlongY[i].y > minY)
					{
						ptY = pointsAlongY[i];
						found++;
						break;
					}
				}
			}
		}

		printf("given: %d, %d; pt1: %d, %d; pt2: %d, %d; found: %d\n", pt.x, pt.y,  ptX.x, ptX.y, ptY.x, ptY.y, found);

		return found;
	}

	Point* getMeetingPoint(Point pointAlongHor, Point pointAlongVer)
	{
		Point* mpt = NULL;
		

		return mpt;
	}

	vector<Rectangle> getRectangles(PointMap& pointMap)
	{
		Rectangle newRect;
		vector<Rectangle> rects;

		for (auto it = pointMap.hPoints.begin(); it != pointMap.hPoints.end(); ++it)
		{
			vector<Point> points = it->second;
			for (uint i = 0; i < points.size(); i++)
			{
				newRect = { points[i].x, points[i].y, 0, 0 };
				vector<Point> pointsAlongX = pointMap.hPoints[newRect.y];
				vector<Point> pointsAlongY = pointMap.vPoints[newRect.x];
				uint direction = DIR_HOR;
				uint curPointXIdx = 0;
				uint curPointYIdx = 0;
				uint curDiagPointXIdx = 0;
				uint curDiagPointYIdx = 0;
				int found = 1;
				int minX = points[i].x;
				int minY = points[i].y;
				Point ptX, ptY, ptDiag;

				while (found > 0 && found < 2)
				{
					found = getNearestPoints(points[i], pointMap, minX, minY, ptX, ptY, minX, minY);
				}
				
				if (found == 2)
				{
					getDiagEndPoint(ptX, ptY, pointMap, ptDiag);
					newRect.w = ptDiag.x - newRect.x;
					newRect.h = ptDiag.y - newRect.y;
				}
			}
		}

		return rects;
	}

	vector<Point> findIntersections(vector<Line> lines)
	{
		vector<Point> points;

		LineMap lineMap = getLineMap(lines);

		for (auto itX = lineMap.hLines.begin(); itX != lineMap.hLines.end(); ++itX)
		{
			vector<Line> currLinesInX = itX->second;
			for (unsigned int i = 0; i < currLinesInX.size(); i++)
			{
				Line currXLine = currLinesInX[i];
				for (auto itY = lineMap.vLines.begin(); itY != lineMap.vLines.end(); ++itY)
				{
					vector<Line> currLinesInY = itY->second;
					for (unsigned int j = 0; j < currLinesInY.size(); j++)
					{
						Line currYLine = currLinesInY[j];

						// the ones or for lines touching not intersecting
						if ((currYLine.start.x <= currXLine.end.x + 1 && currYLine.start.x >= currXLine.start.x - 1)
							&& (currXLine.start.y <= currYLine.end.y + 1 && currXLine.start.y >= currYLine.start.y - 1))
						{
							points.push_back({ currYLine.start.x, currXLine.start.y });
						}
					}
				}
			}
		}

		return points;
	}

	PointMap findIntersections2(vector<Line> lines)
	{
		PointMap pointsMap;

		LineMap lineMap = getLineMap(lines);

		for (auto itX = lineMap.hLines.begin(); itX != lineMap.hLines.end(); ++itX)
		{
			vector<Line> currLinesInX = itX->second;
			for (unsigned int i = 0; i < currLinesInX.size(); i++)
			{
				Line currXLine = currLinesInX[i];
				for (auto itY = lineMap.vLines.begin(); itY != lineMap.vLines.end(); ++itY)
				{
					vector<Line> currLinesInY = itY->second;
					for (unsigned int j = 0; j < currLinesInY.size(); j++)
					{
						Line currYLine = currLinesInY[j];

						// the ones or for lines touching not intersecting
						if ((currYLine.start.x <= currXLine.end.x + 1 && currYLine.start.x >= currXLine.start.x - 1)
							&& (currXLine.start.y <= currYLine.end.y + 1 && currXLine.start.y >= currYLine.start.y - 1))
						{
							pointsMap.hPoints[currXLine.start.y].push_back({ currYLine.start.x, currXLine.start.y });
							pointsMap.vPoints[currYLine.start.x].push_back({ currYLine.start.x, currXLine.start.y });
						}
					}
				}
			}
		}

		return pointsMap;
	}


	vector<Line> extendLinesGreedy(vector<Line>& lines, bool connect, int maxSkipPixels)
	{
		if (lines.size() == 0) return lines;

		vector<Line> newLines;
		Line newLine;
		int direction = 0;
		int currLinePos = -1;
		LineMap lineMap = getLineMap(lines);

		do
		{
			std::map<int, vector<Line>> linesForPos = direction == 0 ? lineMap.hLines : lineMap.vLines;
			std::vector<unsigned int> linesPos = direction == 0 ? lineMap.hLinesPos : lineMap.vLinesPos;

			for (unsigned int i = 0; i < linesPos.size(); i++)
			{
				int currLinePos = linesPos[i];
				// initialize new line with first line in the map
				if (direction == 0)
					newLine = { { linesForPos[currLinePos][0].start.x, currLinePos }, { linesForPos[currLinePos][0].end.x, currLinePos } };
				else
					newLine = { { currLinePos, linesForPos[currLinePos][0].start.y }, { currLinePos, linesForPos[currLinePos][0].end.y } };

				vector<Line> currLines = linesForPos[currLinePos];
				sortByLineStart(currLines, direction);

				for (unsigned int j = 0; j < currLines.size(); j++)
				{

					// check if the gap is not more than specified skip pixels, 
					// if it is, then end this new line and push to the lines vector and create a new line
					if ((direction == 0 ? currLines[j].start.x - newLine.end.x : currLines[j].start.y - newLine.end.y) >= maxSkipPixels)
					{
						newLines.push_back(newLine);
						// create a new line
						if (direction == 0)
							newLine = { { currLines[j].start.x, currLinePos }, { currLines[j].end.x, currLinePos } };
						else
							newLine = { { currLinePos, currLines[j].start.y }, { currLinePos, currLines[j].end.y } };
					}
					// removing this block will make the lines extend greedily

					if (direction == 0)
					{
						newLine.end.x = currLines[j].end.x;
					}
					else
					{
						newLine.end.y = currLines[j].end.y;
					}
				}

				newLines.push_back(newLine);
			}

		} while (++direction < 2);

		return newLines;
	}

	// thickness removal
	vector<Line> removeThickness2(vector<Line>& lines)
	{
		vector<Line> newLines;
		unsigned int *linesToRemove = new unsigned int[lines.size()];
		memset(linesToRemove, 0, sizeof(unsigned int) * lines.size());
		Line sandwich[2];
		int direction = 0;
		int idxc1 = -1;
		int prevIdx = -1;

		for (unsigned int i = 0; i < lines.size(); i++)
		{
			for (unsigned int j = 0; j < lines.size(); j++)
			{

				sandwich[0] = lines.at(i);
				sandwich[1] = lines.at(j);

				// continue if the lines' directions are different
				if (i == j || isDirectionChanged(0, sandwich[0]) != isDirectionChanged(0, sandwich[1]))
					continue;

				if (isThick(sandwich))
				{
					idxc1 = greaterLine(0, 1, sandwich);

					// printf("dbg: thick check: i: %u, idx: %d, prv: %d\n",
					//	i, idxc1, prevIdx);
					//printLine(sandwich[0]);
					//printLine(sandwich[1]);

					// if idx is 0, then remove the second sandwich pointed line
					if (idxc1 == 0)
					{
						linesToRemove[j] = 1;
					}
					else
					{
						linesToRemove[i] = 1;
					}
				}

				// reset for next iteration
				// idxc1 = -1;
			}
		}

		for (unsigned int i = 0; i < lines.size(); i++)
		{
			if (linesToRemove[i] != 1)
				newLines.push_back(lines[i]);
		}

		return newLines;
	}

	// debug recursive line detection
	void debugRecursive(Mat& image)
	{
		cv::Mat linesImage(image);
		char buf[128];
		int skip = 0;
		// VideoWriter video("recursive.avi", CV_FOURCC('M', 'P', '4', '2'), 1, Size(image.cols, image.rows), false);
		for (int i = 5; i < 30; i += 3)
		{
			printf("preparing frame for minlinepx: %d, skippx: %d\n", i, skip);
			vector<Line> lines = getLines(linesImage, skip, i);
			vector<Line> newLines;
			linesImage = cv::Mat::zeros(image.size(), image.type());
			sprintf_s(buf, "min: %d, skip: %d", i, skip);
			putText(linesImage, buf, Point(10, 10), FONT_HERSHEY_PLAIN, 1.0, Scalar(255, 255, 255));
			sprintf_s(buf, "min%dskip%d.bmp", i, skip);
			// remove thickness

			if (i == 11)
			{
				lines = removeThickness2(lines);
				lines = extendLinesGreedy(lines, true, 40);
				lines = extendLinesGreedy(lines, true, 40);
				lines = removeThickness2(lines);
				/*vector<Point> intersects = findIntersections(lines);
				for (unsigned int j = 0; j < intersects.size(); j++)
				{
					cv::drawMarker(linesImage, intersects[j], Scalar(255, 0, 0), MARKER_STAR);
				}*/
				PointMap points = findIntersections2(lines);
				vector<Rectangle> rects = getRectangles(points);
				drawLines(lines, linesImage);
			}
			else
			{
				drawLines(lines, linesImage);
			}

			imwrite(buf, linesImage);

			// remove thicknesses and write the image
			sprintf_s(buf, "min%dskip%dt.bmp", i, skip);
			linesImage = Mat::zeros(linesImage.size(), linesImage.type());
			lines = removeThickness2(lines);
			/*vector<Point> intersects = findIntersections(lines);
			for (unsigned int j = 0; j < intersects.size(); j++)
			{
			cv::drawMarker(linesImage, intersects[j], Scalar(255, 0, 0), MARKER_CROSS, 20, 2);
			}*/
			// lines = extendLinesGreedy(lines, true, 15);
			drawLines(lines, linesImage);
			imwrite(buf, linesImage);

			// video.write(linesImage);

			/*if (skip++ > 7)
			break;*/
		}
		// video.release();
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
	// FIXED: since the skippixels is set, every pixel is considered a line and a line is made with the trailing skipped pixels
	// creating a distorted image
	vector<Line> getLines(Mat &inputImage, unsigned int skipPixels, unsigned int minLinePixels, void(*debug)(Mat output))
	{
		vector<Line> lines;
		// how much continous pixels make a line
		// NOTE: 0 is not an acceptable line
		// its equivalent to allowing a pixel to be set
		//int minLinePixels = 3;
		// how many continous pixels can we skip
		//int skipPixels = 0;
		const int pixelThreshold = 1;
		int width = inputImage.cols;
		int height = inputImage.rows;
		Mat output = cv::Mat::zeros(inputImage.size(), inputImage.type());
		// Mat dbgIm = cv::Mat::zeros(inputImage.size(), inputImage.type());
		bool newLine = false, newLineStarted = false, lineAdded = false;
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
						// count continous blank pixels
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
								currLine.end.x -= (skipPixels + 1);
							else
								currLine.end.y -= (skipPixels + 1);


							// verbose line output
							//printf("line, sx: %d, sy: %d, ex: %d, ey: %d\n", currLine.start.x, currLine.start.y, currLine.end.x, currLine.end.y);
							// detect diagnonals
							if (direction == 1 ? currLine.start.x == currLine.end.x : currLine.start.y == currLine.end.y)
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

					if (newLineStarted == true && (unsigned int)(direction == 0 ? currLine.end.x - currLine.start.x
						: currLine.end.y - currLine.start.y) >= minLinePixels && x + 1 == (direction == 0 ? width : height))
					{
						lines.push_back(currLine);
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

}

int main(int argc, char** argv)
{

	char* imageFileName = argv[1];
	char* outputFileName = "output.bmp";
	if (imageFileName == NULL)
	{
		return 0;
	}

	cv::Mat inputImage = cv::imread(imageFileName);
	cv::Mat outputImage = cv::Mat::zeros(inputImage.size(), inputImage.type());
	cv::Mat grayImage = cv::Mat::zeros(inputImage.size(), inputImage.type());
	cv::Mat linesImage = cv::Mat::zeros(inputImage.size(), inputImage.type());;

	gridcog::convertToGrayScaleAndEdgeDetect(inputImage, grayImage);

	// write edge detected image
	imwrite("edge.bmp", grayImage);

	// vector<Line> lines = getLines(grayImage, 0, 10);
	// drawLines(lines, linesImage);
	//debugGridLines(grayImage);
	gridcog::debugRecursive(grayImage);

	// preview images

	//cv::namedWindow("Original Image", 1);
	//	cv::namedWindow("Lines Image", 1);
	//cv::namedWindow("Filtered Image", 1);

	//cv::imshow("Original Image", inputImage);
	//	cv::imshow("Lines Image", linesImage);
	//cv::imshow("Filtered Image", grayImage);

	cv::waitKey();

	return 0;
}