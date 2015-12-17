#include "ttt/tictactoe_utils.h"

/**************************************************************************/
/**                        CELL                                          **/
/**************************************************************************/

cv::Mat ttt::Cell::mask_image(const cv::Mat img)
{
    cv::Mat mask = cv::Mat::zeros(img.rows, img.cols, CV_8UC1);

    // CV_FILLED fills the connected components found with white
    cv::drawContours(mask, std::vector<std::vector<cv::Point> >(1,contours),
                                            -1, cv::Scalar(255), CV_FILLED);  

    cv::Mat im_crop(img.rows, img.cols, CV_8UC3);                           
    im_crop.setTo(cv::Scalar(0));
    img.copyTo(im_crop, mask);                  

    // normalize so imwrite(...)/imshow(...) shows the mask correctly
    //cv::normalize(mask.clone(), mask, 0.0, 255.0, CV_MINMAX, CV_8UC1);

    // show the images
    // cv::imshow("original", img);
    // cv::imshow("mask", mask);
    // cv::imshow("masked cell", im_crop);

    return im_crop;
}

bool ttt::Cell::get_cell_centroid(cv::Point& centroid)
{
    uint sumX = 0, sumY = 0;
    size_t size = contours.size();
    centroid.x=centroid.y=0;
    if(size > 0){
        for (std::vector<cv::Point>::iterator it_point = contours.begin(); it_point != contours.end(); ++it_point) {
            sumX += it_point->x;
            sumY += it_point->y;
        }
        // TODO through exception if size <= 0
        centroid.x = sumX/size;
        centroid.y = sumY/size;
        return true;
    }
    else
        return false;
}

/**************************************************************************/
/**                                 BOARD                                **/
/**************************************************************************/

std::vector<std::vector<cv::Point> > ttt::Board::as_vector_of_vectors()
{
    std::vector<std::vector<cv::Point> > result;
    
    for (int i = 0; i < cells.size(); ++i)
    {
        result.push_back(cells[i].contours);
    }

    return result;
};

bool ttt::Board::save() 
{
    if (!cells.empty())
    {
        QApplication app(0,0);
        QString fileName = QFileDialog::getSaveFileName(0, "Save File", QDir::currentPath(), "XML files (*.xml)", new QString("XML files (*.xml)"));
        ROS_DEBUG_STREAM("File Name selected= " << fileName.toStdString());
        if(!fileName.isEmpty())
        {
            QFile output(fileName);
            if(!output.open(QIODevice::WriteOnly))
            {
                ROS_WARN("Error opening file to write cells data");
                return false;
            }

            QXmlStreamWriter stream(&output);
            stream.setAutoFormatting(true);
            stream.writeStartDocument();

            stream.writeStartElement("board");

            for (size_t i=0; i< cells.size();++i)
            {
                stream.writeStartElement("cell");
                stream.writeAttribute("id", QString::number(i));
                for (std::vector<cv::Point>::iterator it_vertex = cells[i].contours.begin(); it_vertex != cells[i].contours.end(); ++it_vertex) {
                    stream.writeEmptyElement("vertex");
                    stream.writeAttribute("x", QString::number(it_vertex->x));
                    stream.writeAttribute("y", QString::number(it_vertex->y));
                }
                stream.writeEndElement(); // cell
            }

            stream.writeEndElement(); // board
            stream.writeEndDocument();
            output.close();
        }
        else return false;
    }
    else
    {
        ROS_INFO("No cells in the board to be saved.");
        return false;
    }
    return true;
};

bool ttt::Board::load(std::string cells_param)
{
    ROS_DEBUG("Ready to read xml data of cells");

    std::string xml_cells;

    if (ros::param::get(cells_param, xml_cells)) //reading the xml data from the parameter server
    {
        ROS_INFO("xml cell data successfully loaded");

        cells.clear(); //cleaning all previous cells

        QXmlStreamReader xml;
        xml.addData(QString::fromStdString(xml_cells)); //adding the xml content to the input stream
        while(!xml.atEnd() && !xml.hasError()) {
            /* Read next element.*/
            QXmlStreamReader::TokenType token = xml.readNext();
            ROS_DEBUG_STREAM("name=" << xml.name().toString().toStdString());
            /* If token is just StartDocument, we'll go to next.*/
            if(token == QXmlStreamReader::StartDocument) {
                continue;
            }
            /* If token is StartElement, we'll see if we can read it.*/
            if(token == QXmlStreamReader::StartElement) {
                /* If it's named cell, we'll add a new cell to the board.*/
                if(xml.name() == "cell") {
                    cells.push_back(Cell());
                    continue;
                }
                /* If it's named vertex, we'll dig the information from there.*/
                if(xml.name() == "vertex") {
                    /* Let's get the attributes for vertex */
                    QXmlStreamAttributes attributes = xml.attributes();
                    /* Let's check that vertex has x and y attribute. */
                    if(attributes.hasAttribute("x") && attributes.hasAttribute("y")) {
                        /* We'll add it to the cell */
                        cells.back().contours.push_back(cv::Point(attributes.value("x").toString().toInt(), attributes.value("y").toString().toInt()));
                    }
                    else xml.raiseError("Vertex corrupted: x and/or y value is missing");
                }
            }
        }
        ROS_WARN_COND(xml.hasError(),"Error parsing xml data (l.%d, c.%d):%s", (int)xml.lineNumber(), (int)xml.columnNumber(), xml.errorString().toStdString().c_str());

        ROS_INFO("Xml data successfully loaded. %i cells loaded", (int)cells.size());
        return true;
    }
    else
    {
        ROS_FATAL_STREAM("xml cell data not loaded!");
        return false;
    }
}