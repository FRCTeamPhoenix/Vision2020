

#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableEntry.h>
#include <networktables/NetworkTableInstance.h>
#include <cameraserver/CameraServer.h>
#include <cscore_cpp.h>
#include <cscore_oo.h>
#include <wpi/ArrayRef.h>
#include <wpi/StringRef.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <ctime>
#include <iostream>

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#define LOCALPORT 23422
#define REMOTEPORT 23421

using namespace std;

int network_send(int fd, sockaddr_in addr, char* msg);
int network_recv(int fd, char* buf, size_t len);
vector<string> connect_loop(int fd, sockaddr_in send_addr, char* local_ip, bool& connected);
void display_loop(bool& connected, wpi::ArrayRef<wpi::StringRef>* ips, nt::NetworkTableInstance* ntinst);

int main(int argc, char* argv[]) {
	struct sockaddr_in send_addr, recv_addr, * sa;
	struct ifaddrs* ifap, * ifa;
	char* ifaddr, * local_ip;
	bool connected = false;

	vector<cs::CvSink> streams;

	int trueflag = 1, count = 0;
	int fd;

	getifaddrs(&ifap);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			sa = (struct sockaddr_in*) ifa->ifa_addr;
			ifaddr = inet_ntoa(sa->sin_addr);
			string ipaddr(ifaddr);
			ipaddr = "//" + ipaddr;
			if (ipaddr.find("10") == 2) {
				ipaddr.erase(0, 2);
				local_ip = strdup(ipaddr.c_str());
			}
		}
	}
	freeifaddrs(ifap);

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
	}

	memset(&send_addr, 0, sizeof send_addr);
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = (in_port_t)htons(REMOTEPORT);
	send_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &trueflag, sizeof trueflag) < 0) {
		cerr << "[ERROR] Sockopt recv failed" << endl;
	}

	memset(&recv_addr, 0, sizeof recv_addr);
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = (in_port_t)htons(LOCALPORT);
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr*) & recv_addr, sizeof recv_addr) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
	}

	vector<string> bots = connect_loop(fd, send_addr, local_ip, connected);

	vector<wpi::StringRef> refs;
	for (int i = 0; i < bots.size(); i++) {
		refs.push_back(wpi::StringRef(bots.at(i)));
	}
	wpi::ArrayRef<wpi::StringRef> ips = wpi::ArrayRef(refs);

	nt::NetworkTableInstance ntinst = nt::NetworkTableInstance::GetDefault();
	function<void(const nt::ConnectionNotification&)> connection_listener = [connected, bots, fd, send_addr, local_ip, ntinst](const nt::ConnectionNotification& event) mutable {
		connected = event.connected;
		cout << "[STATUS] Connection status: " << (connected ? "connected" : "disconnected") << endl;
		if (!connected) {
			ntinst.StopClient();
			ntinst.StopDSClient();
			bots = connect_loop(fd, send_addr, local_ip, connected);
			vector<wpi::StringRef> refs;
			for (int i = 0; i < bots.size(); i++) {
				refs.push_back(wpi::StringRef(bots.at(i)));
			}
			wpi::ArrayRef<wpi::StringRef> ips = wpi::ArrayRef(refs);
			display_loop(connected, &ips, &ntinst);
		}
	};

	ntinst.AddConnectionListener(connection_listener, true);
	display_loop(connected, &ips, &ntinst);

	return 0;
}

int network_send(int fd, struct sockaddr_in addr, char* msg) {
	if (sendto(fd, msg, strlen(msg) + 1, 0, (struct sockaddr*) & addr, sizeof addr) < 0) {
		cerr << "[ERROR] Sending failed" << endl;
		return -1;
	}
	return 0;
}

int network_recv(int fd, char* buf, size_t len) {
	if (recv(fd, buf, len - 1, 0) < 0) {
		cerr << "[ERROR] Recieving failed" << endl;
		return -1;
	}
	return 0;
}

vector<string> connect_loop(int fd, sockaddr_in send_addr, char* local_ip, bool& connected) {
	vector<string> bots;
	while (true) {
		char recv_buf[30];
		network_recv(fd, recv_buf, sizeof(recv_buf));
		string recv_msg(recv_buf);
		string identifier = recv_msg.substr(0, recv_msg.find("/") + 4);
		if (identifier == "LOCALIP/BOT") {
			string remote_ip = recv_msg.substr(recv_msg.find(":") + 1);
			bots.push_back(remote_ip);

			cout << "[STATUS] Connected to " << remote_ip << endl;
			connected = true;

			char send_msg[30];
			sprintf(send_msg, "LOCALIP/SERVER:%s", local_ip);
			inet_pton(AF_INET, remote_ip.c_str(), &(send_addr.sin_addr));
			network_send(fd, send_addr, send_msg);
			usleep(1000000);

			if (bots.size() == 1) return bots;
		}
	}
}

void display_loop(bool& connected, wpi::ArrayRef<wpi::StringRef>* ips, nt::NetworkTableInstance* ntinst) {
	ntinst->SetServer(*ips, 23420);
	ntinst->StartClient();
	ntinst->StartDSClient();

	vector<cs::CvSink> streams;
	vector<cs::HttpCamera> cams;

	for (int i = 0; i < ips->size(); i++) {
		cams.push_back(cs::HttpCamera("Cam" + to_string(i), "http://" + ips->vec().at(i) + ":2342/stream.mjpg"));
		cams.at(i).SetResolution(720, 480);
		cams.at(i).SetFPS(30);
		cs::CvSink stream = cs::CvSink("Stream" + to_string(i));
		stream.SetSource(cams.at(i));
		streams.push_back(stream);
	}

	cs::CvSink stream = streams.at(0);

	cout << "Showing stream" << endl;
	cv::Mat frame;
	while (connected) {
		for (int i = 0; i < streams.size(); i++) {
			cs::CvSink stream = streams.at(i);
			stream.GrabFrame(frame);
			if (!frame.empty()) {
				cv::imshow("Stream" + to_string(i), frame);
			}
			if (cv::waitKey(5) >= 0) {
				break;
			}
		}
	}
	cams.clear();
	streams.clear();
}