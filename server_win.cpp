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
#define LOCALPORT 23421
#define REMOTEPORT 23422

using namespace std;

int network_send(int fd, sockaddr_in addr, char* msg);
int network_recv(int fd, sockaddr_in addr, char* buf, size_t len);
int connect_loop(int fd, sockaddr_in send_addr, sockaddr_in recv_addr, string bot, string local_ip, string &remote_ip);
int get_ip(string& ip);

int main(int argc, char* argv[]) {
	string local_ip, server_ip, bot = "0";
	struct sockaddr_in send_addr, recv_addr;
	bool connected = false;
	BOOL trueflag = TRUE;
	int fd;
	WSADATA WsaData;
	PCSTR opt;
	cv::Mat frame;
	int tv = 1000;

	if (WSAStartup(MAKEWORD(1, 1), &WsaData) < 0) {
		cout << "[ERROR] WSA Startup error" << endl;
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *)&trueflag, sizeof(trueflag)) < 0) {
		cerr << "[ERROR] Sockopt broadcast failed" << endl;
		return -1;
	}

	if (get_ip(local_ip) < 0) {
		cerr << "[ERROR] Could not retrieve IP Address" << endl;
		return -1;
	}

	memset(&send_addr, 0, sizeof send_addr);
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = (u_short)htons(REMOTEPORT);
	send_addr.sin_addr.s_addr = INADDR_BROADCAST;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&trueflag, sizeof(trueflag)) < 0) {
		cerr << "[ERROR] Sockopt recv failed" << endl;
		return -1;
	}

	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = (u_short)htons(LOCALPORT);
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(fd, (struct sockaddr*) &recv_addr, sizeof(recv_addr)) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)) < 0) {
		cerr << "[ERROR] Sockopt timeout failed" << endl;
		return -1;
	}

	nt::NetworkTableInstance ntinst = nt::NetworkTableInstance::GetDefault();
	ntinst.StartServer("networktables.ini", local_ip.c_str(), 23420);
	function<void(const nt::ConnectionNotification&)> connection_listener = [connected, fd, send_addr, recv_addr, server_ip, bot, local_ip](const nt::ConnectionNotification& event) mutable {
		connected = event.connected;
		cout << "[STATUS] Connection status: " << (connected ? "connected" : "disconnected") << endl;

	};
	ntinst.AddConnectionListener(connection_listener, true);

	cs::UsbCamera camera = frc::CameraServer::GetInstance()->StartAutomaticCapture();
	camera.SetResolution(720, 480);

	cs::CvSink sink = frc::CameraServer::GetInstance()->GetVideo();
	cs::CvSource output = frc::CameraServer::GetInstance()->PutVideo("Source" + bot, 720, 480);
	cs::MjpegServer server = cs::MjpegServer("Bot" + bot, 2342);
	server.SetResolution(720, 480);
	server.SetCompression(80);
	server.SetFPS(30);
	server.SetSource(output);

	connect_loop(fd, send_addr, recv_addr, bot, local_ip, server_ip);

	while (true) {
		sink.GrabFrame(frame);
		if (frame.empty()) {
			cerr << "[ERROR] Blank frame grabbed." << endl;
		}
		else {
			output.PutFrame(frame);
		}
	}

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

int network_recv(int fd, struct sockaddr_in addr, char* buf, size_t len) {
	if (recv(fd, buf, len - 1, 0) < 0) {
		return -1;
	}
	return 0;
}

int connect_loop(int fd, struct sockaddr_in send_addr, struct sockaddr_in recv_addr, string bot, string local_ip, string &remote_ip) {
	while (true) {
		char send_msg[50];
		sprintf(send_msg, "LOCALIP/BOT%s:%s", bot.c_str(), local_ip.c_str());
		network_send(fd, send_addr, send_msg);

		Sleep(100);

		char recv_msg[50];
		network_recv(fd, recv_addr, recv_msg, 50);
		cout << recv_msg << endl;
		string recv_str(recv_msg);
		string identifier = recv_str.substr(0, recv_str.find(":"));
		string ip_str = recv_str.substr(recv_str.find(":") + 1);
		if (identifier == "LOCALIP/SERVER") {
			cout << "[STATUS] Connected to " + ip_str << endl;
			remote_ip.assign(ip_str);
			return 0;
		}
	}
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

	if (pIPAddrTable) {
		for (int i = 0; i < (int)pIPAddrTable->dwNumEntries; i++) {
			IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwAddr;
			string local_ip(inet_ntoa(IPAddr));
			local_ip = "//" + local_ip;
			if (local_ip.find("10") == 2) {
				local_ip.erase(0, 2);
				ip = local_ip;
				cout << "[STATUS] Local IP: " << ip << endl;
			}
		}
	}


	if (pIPAddrTable) {
		FREE(pIPAddrTable);
		pIPAddrTable = NULL;
	}

	return 0;
}
