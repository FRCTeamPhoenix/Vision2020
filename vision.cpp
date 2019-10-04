#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableEntry.h>
#include <networktables/NetworkTableInstance.h>
#include <cameraserver/CameraServer.h>
#include <cscore_cpp.h>
#include <cscore_oo.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <stdio.h>

using namespace std;
using namespace cv;
using namespace frc;
using namespace cs;

int main(int argc, char* argv[]) {
    Mat frame;
    UsbCamera camera = CameraServer::GetInstance()->StartAutomaticCapture();
    camera.SetResolution(1920, 1080);
    CvSink sink = CameraServer::GetInstance()->GetVideo();
    CvSource output = CameraServer::GetInstance()->PutVideo("Live", 1920, 1080);

    while (true) {
        sink.GrabFrame(frame);
        if (frame.empty()) {
            cerr << "ERROR! Blank frame grabbed\n";
            break;
        }
        output.PutFrame(frame);
        if (waitKey(5) >= 0) {
            break;
        }
    }
    return 0;
}