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

    vector<string> bots{"0", "1", "2"};
    vector<cs::CvSink> streams;

    nt::NetworkTableInstance ntinst = nt::NetworkTableInstance::GetDefault();
    ntinst.SetServer("127.0.0.1", 23425);
    ntinst.StartClient();
    ntinst.StartDSClient();

    for (string bot : bots) {
        cs::HttpCamera cam = cs::HttpCamera("Cam" + bot, "http://127.0.0.1:2342" + bot + "/stream.mjpg");
        cam.SetResolution(720, 480);
        cam.SetFPS(30);
        cs::CvSink stream = cs::CvSink("Stream" + bot);
        stream.SetSource(cam);
        streams.push_back(stream);
    }

    cs::CvSink stream = streams.at(0);

    while (true) {  
        stream.GrabFrame(frame);
        if (frame.empty()) {
            cerr << "[ERROR] Blank frame grabbed." << endl;
        } else {
            cv::imshow("Live", frame);  
        }
        if (cv::waitKey(5) >= 0) {
            break;
        }
    }
    return 0;
}