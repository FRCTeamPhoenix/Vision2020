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

int main(int argc, char* argv[]) {
    cv::Mat frame;
    cs::UsbCamera camera = frc::CameraServer::GetInstance()->StartAutomaticCapture();
    camera.SetResolution(720, 480);
    cs::CvSink sink = frc::CameraServer::GetInstance()->GetVideo();
    cs::CvSource output = frc::CameraServer::GetInstance()->PutVideo("Live", 720, 480);
    cs::MjpegServer server = cs::MjpegServer("Team2342", 2342);
    
    server.SetResolution(720, 480);
    server.SetCompression(80);
    server.SetFPS(30);
    server.SetSource(output);

    while (true) {
        sink.GrabFrame(frame);
        if (frame.empty()) {
            cerr << "[ERROR] Blank frame grabbed." << endl;
        } else {
            output.PutFrame(frame);
        }
        if (cv::waitKey(5) >= 0) {
            break;
        }
    }
    return 0;
}