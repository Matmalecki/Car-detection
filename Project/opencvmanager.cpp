#include "opencvmanager.h"

using namespace cv;
using namespace std;

OpencvManager::OpencvManager(QObject * parent) : QObject (parent), status(false), toggleStream(false), color(122,255,255)
{

    cap = new VideoCapture();

    bgsubtractor = createBackgroundSubtractorMOG2();
    bgsubtractor->setNMixtures(100);
    bgsubtractor->setShadowValue(0);



}

OpencvManager::~OpencvManager()
{
    if (cap->isOpened()) cap->release();
    delete cap;
    image.release();
    bgsubtractor.release();
}

void OpencvManager::checkIfDeviceAlreadyOpened(QByteArray device)
{

    if (cap->isOpened()) cap->release();
    if (strcmp(device, "0") == 0) cap->open(0);
    else {
       cap->open(device.toStdString().c_str());
    }
}


void OpencvManager::receiveGrabFrame()
{
    if (!toggleStream || !status)
    {
        return;
    }
    cap->read(image);
    if (image.empty()) {
        cap->set(CV_CAP_PROP_POS_FRAMES, 0);
        cap->read(image);
    }

    process();

    QImage outputSource((const unsigned char *)image.data, image.cols, image.rows, QImage::Format_RGB888);
    emit sendSourceFrame(outputSource);

}


void OpencvManager::process()
{
     Scalar lineColor(100,200,110);
     vector<Person> currentFramePeople;
     Mat processed;
     bgsubtractor->apply(image, processed);

     threshold(processed, processed,55,255, THRESH_BINARY);

     morphologyEx(processed, processed, MORPH_OPEN, getStructuringElement(0,Size(3,3)));



     vector<vector<Point> > contours;
     vector<Vec4i> hierarchy;
     findContours(processed, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

     if (contours.size() > 0)
     {
         vector<Rect> boundingRects;
         vector<Moments> mu;


         for (int i = 0 ; i < contours.size(); i++)
         {
            drawContours(processed, contours, i, Scalar(255,255,255), CV_FILLED);
         }
         blur(processed, processed, Size(5,5));

         for (int i = 0; i < contours.size(); i++)
         {
             // skip non-human objects
             if (contourArea(contours[i]) > HUMAN_SIZE_MIN && contourArea(contours[i]) < HUMAN_SIZE_MAX )
             {
                boundingRects.push_back(boundingRect(contours[i]));
                mu.push_back(moments(contours[i],true));
             }
         }

          vector<Point2f> momentsCenters( mu.size() );

          for(int i = 0; i < mu.size(); i++ )
          {
              momentsCenters[i] = Point2f( mu[i].m10/mu[i].m00 , mu[i].m01/mu[i].m00 );
          }

          for( int i = 0; i < mu.size(); i++ )
          {
               currentFramePeople.push_back(Person(boundingRects[i], momentsCenters[i]));
          }


          for (auto& person : people)
          {
              person.active = false;
          }

        for (auto& person : currentFramePeople)
        {
            bool deletePerson = false;
            // check for existing
            for (int i = 0; i < people.size(); i++)
            {
                if (norm(person.centerPositions - people[i].centerPositions) <= person.currentRect.width/2 ||
                        norm(person.centerPositions - people[i].centerPositions) <= person.currentRect.height/2)
                {
                    if (people[i].active){

                        deletePerson = true;
                        break;
                    }
                    if (!people[i].counted)
                        if (checkIfCrossedLine(people[i].origin, person.centerPositions))
                        {
                            person.counted = true;
                            lineColor = Scalar(100,100,200);
                        }
                    person.personId = people[i].personId;
                    people[i] = person;
                    people[i].active = true;
                }
            }

            // Add new person
            if (person.personId == 0 && !deletePerson)
            {
                person.active = true;
                person.personId = nextId;
                nextId++;
                if (nextId > 100)
                    nextId = 1;
                people.push_back(person);
            }
        }
        // remove those that are no longer in image
        for (int i = 0; i < people.size(); i++)
        {
            if (!people[i].active)
            {
                people.erase(people.begin()+i);
            }
        }

        // draw
        for (auto& person : people)
        {
            rectangle(image,person.currentRect.tl(),person.currentRect.br(),color);
            circle(image, person.centerPositions,1, color);
        }

     }

     line(image, Point(0,image.rows/2), Point(image.cols, image.rows/2),lineColor,2);

     currentFramePeople.clear();
     if (debugWindow)
        imshow("debug", processed);
     processed.release();

}

bool OpencvManager::checkIfCrossedLine(Point prev, Point next){

    // Going up
    if (prev.y > image.cols/2 - 30 && next.y <= image.cols/2 - 30)
    {
        if (isCounting){
            countUp++;
            emit sendUpCounter(countUp);
        }
        return true;
    }
    // going down
    else if (prev.y < image.cols/2 - 40 && next.y >= image.cols/2 - 40)
    {
         if (isCounting) {
            countDown++;
            emit sendDownCounter(countDown);
         }
         return true;
    }
    return false;

}

void OpencvManager::receiveSetup(QByteArray device)
{
    checkIfDeviceAlreadyOpened(device);
    if (!cap->isOpened())
    {
        status = false;
        return;
    }
    status = true;
}

void OpencvManager::receiveToggleStream()
{
    toggleStream = !toggleStream;
}

void OpencvManager::receiveToggleDebug()
{
    debugWindow = !debugWindow;
    if (!debugWindow)
        destroyAllWindows();
}


void OpencvManager::receiveIsCounting(bool shouldCount)
{
    this->isCounting = shouldCount;
}


void OpencvManager::receiveClearCount()
{
    this->countUp = 0;
    this->countDown = 0;
}

