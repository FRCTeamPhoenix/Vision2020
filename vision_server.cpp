#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableEntry.h>
#include <networktables/NetworkTableInstance.h>
#include <cameraserver/CameraServer.h>
#include <cscore_cpp.h>
#include <cscore_oo.h>
#include <ntcore_cpp.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <stdio.h>

using namespace std;

int main(int argc, char* argv[]) {
    cv::Mat frame;
    bool connected = false;

    string bot = "0";

    nt::NetworkTableInstance ntinst = nt::NetworkTableInstance::GetDefault();
    ntinst.StartServer("networktables.ini", "127.0.0.1", 23425);
    std::function<void(const nt::ConnectionNotification&)> connection_listener = [connected](const nt::ConnectionNotification& event) mutable { connected = event.connected; cout << "[STATUS] Connection status: " << (event.connected ? "connected" : "disconnected") << endl; };
    ntinst.AddConnectionListener(connection_listener, true);

    cs::UsbCamera camera = frc::CameraServer::GetInstance()->StartAutomaticCapture();
    camera.SetResolution(720, 480);

    cs::CvSink sink = frc::CameraServer::GetInstance()->GetVideo();
    cs::CvSource output = frc::CameraServer::GetInstance()->PutVideo("Source" + bot, 720, 480);
    cs::MjpegServer server = cs::MjpegServer("Server" + bot, 23420 + stoi(bot));
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
    }
    return 0;
}
