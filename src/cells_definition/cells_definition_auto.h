/*
 * cellsDefinition
 * ------------------------------
 * 
 * cellsDefinition identifies and then advertises the boundaries (contours) 
 * of the tictactoe board and its cells
 * 
 */ 

#ifndef CELLS_DEFINITION_AUTO_H
#define CELLS_DEFINITION_AUTO_H

#include <pthread.h>

#include <string>

#include <ros/ros.h>
#include <ros/console.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <QFile>
#include <QXmlStreamWriter>
#include <QFileDialog>
#include <QApplication>

#include "baxterTictactoe/tictactoe_utils.h"

#include "baxter_tictactoe/DefineCells.h"
#include "baxter_tictactoe/MsgCell.h"
#include "baxter_tictactoe/MsgBoard.h"
#include "baxter_tictactoe/Point.h"

enum Index 
{
	LARGEST 		= 1,
	NEXT_LARGEST    = 2
};

class cellsDefinition
{

private:

	ros::NodeHandle node_handle;
	image_transport::ImageTransport image_transport;
	image_transport::Subscriber image_subscriber;

    ros::ServiceServer service;

	std::string window_name;
	ttt::Cell cell;
	ttt::Board board;

    bool img_loaded;

    pthread_mutex_t mutex;

    /**
     * @param      vector (i.e array) of contours, type indicating whether largest or  
     * 			   next largest area is to be found where (LARGEST = largest area, 
     * 			   NEXT_LARGEST = next largest area)
     *                
     * @return     index of the contour with the largest area or the next largest area
     */
	int getIthIndex(std::vector<std::vector<cv::Point> > contours, Index ith);
    
    /**
     * @param      contour 
     * 
     * @return     centroid of the contour
     */
	cv::Point findCentroid(std::vector<cv::Point> contour);

public:

	cellsDefinition();
	~cellsDefinition();
    
    /* mouse event handler function */
    static void onMouseClick( int event, int x, int y, int, void* param);

    /**
     * callback function executed whenever a new message (raw image) from the usb_cam node is received
     * and identifies the boundaries of the tictactoe board and its cells
     * 
     * @param      raw image from camera
     * 
     * @return     N/A
     */    
	void imageCallback(const sensor_msgs::ImageConstPtr& msg);

    /**
     * service that provides data on the defined contours/boundaries of the board's cells
     * 
     * @param      request variables and response variables 
     * 
     * @return     returns true when function is succesfully executed
     */
    bool defineCells(baxter_tictactoe::DefineCells::Request  &req, 
                     baxter_tictactoe::DefineCells::Response &res);
}; 

#endif //CELLS_DEFINITION_AUTO_H
