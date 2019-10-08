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
    nt::NetworkTableInstance ntinst = nt::NetworkTableInstance::GetDefault();
    ntinst.SetServerTeam(2342);
    ntinst.StartServer();

    while (true) {
        if (cv::waitKey(5) >= 0) {
            break;
        }
    }
    return 0;
}