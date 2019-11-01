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
#include <WS2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <synchapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#define LOCALPORT 23422
#define REMOTEPORT 23421

using namespace std;

int network_send(int fd, sockaddr_in addr, char* msg);
int network_recv(int fd, char* buf, size_t len);
vector<string> connect_loop(int fd, sockaddr_in send_addr, char* local_ip, bool& connected);
void display_loop(bool& connected, wpi::ArrayRef<wpi::StringRef>* ips, nt::NetworkTableInstance* ntinst);
int get_ip(string& ip);

int main(int argc, char* argv[]) {
	struct sockaddr_in send_addr, recv_addr;
	vector<cs::CvSink> streams;
	bool connected = false;
	int trueflag = 1, fd;
	WSADATA WsaData;
	char* local_ip = (char*)MALLOC(sizeof(char));
	string temp_ip;

	if (WSAStartup(MAKEWORD(1, 1), &WsaData) < 0) {
		cout << "[ERROR] WSA Startup error" << endl;
	}

	get_ip(temp_ip);
	if (local_ip != 0) strcpy(local_ip, temp_ip.c_str());

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
	}

	memset(&send_addr, 0, sizeof send_addr);
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = (u_short)htons(REMOTEPORT);
	send_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&trueflag, sizeof trueflag) < 0) {
		cerr << "[ERROR] Sockopt recv failed" << endl;
	}

	memset(&recv_addr, 0, sizeof recv_addr);
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = (u_short)htons(LOCALPORT);
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(fd, (struct sockaddr*) & recv_addr, sizeof recv_addr) == SOCKET_ERROR) {
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

	WSACleanup();
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
			Sleep(1000);

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
	return;
}

int get_ip(string& ip) {
	PMIB_IPADDRTABLE pIPAddrTable;
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;
	IN_ADDR IPAddr;
	LPVOID lpMsgBuf;

	pIPAddrTable = (MIB_IPADDRTABLE*)MALLOC(sizeof(MIB_IPADDRTABLE));

	if (pIPAddrTable) {
		if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) ==
			ERROR_INSUFFICIENT_BUFFER) {
			FREE(pIPAddrTable);
			pIPAddrTable = (MIB_IPADDRTABLE*)MALLOC(dwSize);
		}
		if (pIPAddrTable == NULL) {
			exit(1);
		}
	}

	if ((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0)) != NO_ERROR) {
		return 1;
	}

	for (int i = 0; i < (int)pIPAddrTable->dwNumEntries; i++) {
		IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwAddr;
		string local_ip(inet_ntoa(IPAddr));
		local_ip = "//" + local_ip;
		if (local_ip.find("10") == 2) {
			local_ip.erase(0, 2);
			ip = local_ip;
		}
	}

	if (pIPAddrTable) {
		FREE(pIPAddrTable);
		pIPAddrTable = NULL;
	}

	return 0;
}
