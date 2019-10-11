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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctime>
#include <iostream>
#include <unistd.h>
#include <ifaddrs.h>

#define SERVERPORT 23421
#define REMOTEPORT 23422

using namespace std;

int network_send(int fd, sockaddr_in addr, char* msg);
int network_recv(int fd, sockaddr_in addr, char* buf, size_t len);
string connect_loop(int fd, sockaddr_in send_addr, sockaddr_in recv_addr, string bot, string local_ip);

int main(int argc, char* argv[]) {
    struct sockaddr_in send_addr, recv_addr;
    struct sockaddr_in *sa;
    struct ifaddrs *ifap, *ifa;
    string local_ip, server_ip;
    const char* ex = "lo";
    char* excl = (char *) ex;
    char* local_addr;

    bool connected = false;
    string bot = "0";   
    cv::Mat frame;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 500000;
    int trueflag = 1;
    int fd;

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family==AF_INET && *ifa->ifa_name != *excl) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            local_addr = inet_ntoa(sa->sin_addr);
            string ipaddr(local_addr);
            ipaddr = "//" + ipaddr;
            if (ipaddr.find("10") == 2) {
                ipaddr.erase(0, 2);
                local_ip.assign(ipaddr);
            }
        }
    }
    freeifaddrs(ifap);

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "[ERROR] Socket binding failed" << endl;
        return -1;
    }

    #ifndef RECV_ONLY
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &trueflag, sizeof trueflag) < 0) {
        cerr << "[ERROR] Sockopt broadcast failed" << endl;
        return -1;
    }

    memset(&send_addr, 0, sizeof send_addr);
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = (in_port_t) htons(REMOTEPORT);
    // broadcasting address for unix (?)
    inet_aton("10.0.0.255", &send_addr.sin_addr);
    // send_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    #endif // ! RECV_ONLY

    #ifndef SEND_ONLY
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &trueflag, sizeof trueflag) < 0) {
        cerr << "[ERROR] Sockopt recv failed" << endl;
        return -1;
    }

    memset(&recv_addr, 0, sizeof recv_addr);
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = (in_port_t) htons(SERVERPORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr*) &recv_addr, sizeof recv_addr) < 0) {
        cerr << "[ERROR] Socket binding failed" << endl;
        return -1;
    }    
    #endif // ! SEND_ONLY

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) < 0) {
        cerr << "[ERROR] Sockopt timeout failed" << endl;
        return -1;
    }

    nt::NetworkTableInstance ntinst = nt::NetworkTableInstance::GetDefault();
    ntinst.StartServer("networktables.ini", local_ip.c_str(), 23420);
    function<void(const nt::ConnectionNotification&)> connection_listener = [connected, fd, send_addr, recv_addr, server_ip, bot, local_ip](const nt::ConnectionNotification& event) mutable { 
        connected = event.connected; 
        cout << "[STATUS] Connection status: " << (connected ? "connected" : "disconnected") << endl; 
        server_ip.assign(connect_loop(fd, send_addr, recv_addr, bot, local_ip));
    };
    ntinst.AddConnectionListener(connection_listener, true);

    server_ip.assign(connect_loop(fd, send_addr, recv_addr, bot, local_ip));

    cs::UsbCamera camera = frc::CameraServer::GetInstance()->StartAutomaticCapture();
    camera.SetResolution(720, 480);

    cs::CvSink sink = frc::CameraServer::GetInstance()->GetVideo();
    cs::CvSource output = frc::CameraServer::GetInstance()->PutVideo("Source" + bot, 720, 480);
    cs::MjpegServer server = cs::MjpegServer("Bot" + bot, 2342);
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

int network_send(int fd, struct sockaddr_in addr, char* msg) {
    #ifndef RECV_ONLY
    if (sendto(fd, msg, strlen(msg)+1, 0, (struct sockaddr*) &addr, sizeof addr) < 0) {
        cerr << "[ERROR] Sending failed" << endl;
        return -1;
    }        
    return 0;
    #endif // ! RECV_ONLY
}

int network_recv(int fd, struct sockaddr_in addr, char* buf, size_t len) {
    #ifndef SEND_ONLY
    if (recv(fd, buf, len-1, 0) < 0) {
        return -1;
    }
    return 0;
    #endif // ! SEND_ONLY
}

string connect_loop(int fd, struct sockaddr_in send_addr, struct sockaddr_in recv_addr, string bot, string local_ip) {
    while (true) {
        char send_msg[50];
        sprintf(send_msg, "LOCALIP/BOT%s:%s", bot.c_str(), local_ip.c_str());
        network_send(fd, send_addr, send_msg);
        
        usleep(1000000/2);

        char recv_msg[50];
        network_recv(fd, recv_addr, recv_msg, sizeof(recv_msg));
        string recv_str(recv_msg);
        string identifier = recv_str.substr(0, recv_str.find(":"));
        string remote_ip = recv_str.substr(recv_str.find(":") + 1);

        if (identifier == "LOCALIP/SERVER")  {
            cout << "[STATUS] Connected to " + remote_ip << endl;
            return remote_ip;
        }
    }
}
