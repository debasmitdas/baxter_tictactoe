#ifndef TTT_DEFINITIONS_H
#define TTT_DEFINITIONS_H

#include <ros/ros.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QFileDialog>
#include <QApplication>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/core/core.hpp>

#include "baxter_tictactoe/MsgCell.h"


#include <sstream>

namespace enc = sensor_msgs::image_encodings;

namespace ttt
{

// Used to determine the three possible states of a cell.
// Undefined is just used when it is created and non state has been assigned.
typedef enum {empty=baxter_tictactoe::MsgCell::EMPTY,
              blue=baxter_tictactoe::MsgCell::BLUE,
              red=baxter_tictactoe::MsgCell::RED,
              undefined=baxter_tictactoe::MsgCell::UNDEFINED} cellState;

std::string cell_state_to_str(cellState c_s);

const std::size_t NUMBER_OF_CELLS=9;

struct colorRange {
    int min;
    int max;

    colorRange(): min(0), max(0) {};
    colorRange(int _min, int _max) : min(_min), max(_max) {};
    colorRange(const colorRange &_cr): min(_cr.min), max(_cr.max) {};

    /**
    * Copy Operator
    **/
    colorRange &operator=(const colorRange &);
};

struct hsvColorRange {
    colorRange H;
    colorRange S;
    colorRange V;

    hsvColorRange(): H(0,180), S(0,256), V(0,256) {};
    hsvColorRange(const colorRange &_H, const colorRange &_S,
                 const colorRange &_V) : H(_H), S(_S), V(_V) {};
    hsvColorRange(XmlRpc::XmlRpcValue);

    cv::Scalar get_hsv_min() { return cv::Scalar(H.min, S.min, V.min); };
    cv::Scalar get_hsv_max() { return cv::Scalar(H.max, S.max, V.max); };

    std::string toString();

    /**
    * Copy Operator
    **/
    hsvColorRange &operator=(const hsvColorRange &);
};

struct Cell
{
public:
    std::vector<cv::Point> contours;
    cv::Point centroidCE;
    cellState state;
    double cell_area_red;
    double cell_area_blue;

public:
    Cell() { state=empty; cell_area_red=0; cell_area_blue=0; };
    Cell(std::vector<cv::Point> _vec) : contours(_vec) {};
    ~Cell() {};

    std::vector<cv::Point> get_contours() { return contours; };
    
    /**
     * Returns a mask for a cell, i.e. a new image that keeps only the portion
     * delimited by the cell (the rest is set to black)
     * @param      the original image, where the mask will be extracted from.
     * @return     the mask. It has the same size than the original image.
     */
    cv::Mat mask_image(const cv::Mat &);

    /**
     * Computes the centroid of a cell.
     * @param  centroid the centroid
     * @return          true/false if success/failure.
     */
    bool get_cell_centroid(cv::Point& centroid);

    /**
     * Prints some useful information from the cell
     * @return the string with the information
     */
    std::string toString();
};

struct Board
{
public: 
    std::vector<ttt::Cell> cells;

public:
    Board()  {};
    ~Board() {};

    bool resetState();

    std::string stateToString();

    std::vector<std::vector<cv::Point> > as_vector_of_vectors();

    /**
     * Saves the board to file (whose name will be specified with a dialog window).
     * The format of the file is as follows:
     *   <board>
     *     <cell id="0">
     *       <vertex x="195" y="50"/>
     *       [...]
     *     </cell>
     *     [...]
     *  </board>
     * @return true/false if success/failure.
     */
    bool save();

    /**
     * Reads the contours of the cells from the parameter server,
     * This data is formatted as a xml file, with the following format:
     *   <board>
     *     <cell id="0">
     *       <vertex x="195" y="50"/>
     *       [...]
     *     </cell>
     *     [...]
     *  </board>
     * @param  cells_param the name of the parameter where the raw data is stored. 
     * @return true/false if success/failure.
     */
    bool load(std::string cells_param);

    /**
     * Returns a mask for a board, i.e. a new image that keeps only the portion
     * delimited by the board (the rest is set to black)
     * @param      the original image, where the mask will be extracted from.
     * @return     the mask. It has the same size than the original image.
     */
    cv::Mat mask_image(const cv::Mat &);
};

/**
 * This function thresholds the HSV image and create a binary image
 * @param  img_hsv [description]
 * @param  lower   [description]
 * @param  upper   [description]
 * @return         [description]
 */
cv::Mat hsv_threshold(const cv::Mat& _src, hsvColorRange _hsv);

}

#endif // TTT_DEFINITIONS_H
