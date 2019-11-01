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
string connect_loop(int fd, sockaddr_in send_addr, sockaddr_in recv_addr, string bot, string local_ip);
int get_ip(string& ip, string& braddr);

int main(int argc, char* argv[]) {
	string local_ip, server_ip, br_addr, bot = "0";
	struct sockaddr_in send_addr, recv_addr;
	bool connected = false;
	char trueflag = 'a';
	int fd;
	WSADATA WsaData;
	PCSTR opt;
	cv::Mat frame;
	int tv = 500;

	if (WSAStartup(MAKEWORD(1, 1), &WsaData) < 0) {
		cout << "[ERROR] WSA Startup error" << endl;
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &trueflag, sizeof(trueflag)) < 0) {
		cerr << "[ERROR] Sockopt broadcast failed" << endl;
		return -1;
	}

	if (get_ip(local_ip, br_addr) < 0) {
		cerr << "[ERROR] Could not retrieve IP Address" << endl;
		return -1;
	}

	memset(&send_addr, 0, sizeof send_addr);
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = (u_short)htons(REMOTEPORT);
	opt = (PCSTR)br_addr.c_str();
	InetPtonA(AF_INET, opt, &send_addr.sin_addr);

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &trueflag, sizeof(trueflag)) < 0) {
		cerr << "[ERROR] Sockopt recv failed" << endl;
		return -1;
	}

	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = (u_short)htons(LOCALPORT);
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(fd, (struct sockaddr*) & recv_addr, sizeof recv_addr) < 0) {
		cerr << "[ERROR] Socket binding failed" << endl;
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(const char*)) < 0) {
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

string connect_loop(int fd, struct sockaddr_in send_addr, struct sockaddr_in recv_addr, string bot, string local_ip) {
	while (true) {
		char send_msg[50];
		sprintf(send_msg, "LOCALIP/BOT%s:%s", bot.c_str(), local_ip.c_str());
		network_send(fd, send_addr, send_msg);

		Sleep(1000);

		char recv_msg[50];
		network_recv(fd, recv_addr, recv_msg, sizeof(recv_msg));
		string recv_str(recv_msg);
		string identifier = recv_str.substr(0, recv_str.find(":"));
		string remote_ip = recv_str.substr(recv_str.find(":") + 1);

		if (identifier == "LOCALIP/SERVER") {
			cout << "[STATUS] Connected to " + remote_ip << endl;
			return remote_ip;
		}
	}
}

int get_ip(string& ip, string& braddr) {
	PIP_ADAPTER_ADDRESSES IPAddrs, CurAddrs;
	PIP_ADAPTER_UNICAST_ADDRESS localAddr;
	PIP_ADAPTER_MULTICAST_ADDRESS brAddr;
	ULONG bufSize = 15000, tries = 0;
	DWORD dwSize = 0;
	DWORD dwRetVal = 0;
	IN_ADDR IPAddr;
	LPVOID lpMsgBuf;

	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	IPAddrs = (IP_ADAPTER_ADDRESSES*)MALLOC(sizeof(IP_ADAPTER_ADDRESSES));
	CurAddrs = (IP_ADAPTER_ADDRESSES*)MALLOC(sizeof(IP_ADAPTER_ADDRESSES));

	do {
		IPAddrs = (IP_ADAPTER_ADDRESSES*)MALLOC(bufSize);
		if (IPAddrs == NULL) {
			cerr << "[ERROR] MALLOC for IPAddrs failed" << endl;
			return -1;
		}

		dwRetVal = GetAdaptersAddresses(AF_INET, flags, NULL, IPAddrs, &bufSize);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			FREE(IPAddrs);
			IPAddrs = NULL;
		}
		else {
			break;
		}
		tries++;
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (tries < 10));


	if (dwRetVal == NO_ERROR) {
		CurAddrs = IPAddrs;
		while (CurAddrs) {
			string local_addr, br_addr;
			bool valid_ip = false;
			localAddr = CurAddrs->FirstUnicastAddress;
			if (localAddr) {
				for (int i = 0; localAddr != NULL; i++) {
					char* ipChar = inet_ntoa(((sockaddr_in*)localAddr->Address.lpSockaddr)->sin_addr);
					string ip_str(ipChar);
					ip_str = "//" + ip_str;
					if (ip_str.find("10") == 2) {
						ip_str.erase(0, 2);
						local_addr.assign(ip_str);
						valid_ip = true;
					}
					localAddr = localAddr->Next;
				}
			}
			brAddr = CurAddrs->FirstMulticastAddress;
			if (brAddr) {
				for (int i = 0; brAddr != NULL; i++) {
					char* brChar = inet_ntoa(((sockaddr_in*)brAddr->Address.lpSockaddr)->sin_addr);
					string br_str(brChar);
					br_str = "//" + br_str;
					if (br_str.find("224") != 2 && valid_ip) {
						br_str.erase(0, 2);
						br_addr.assign(br_str);
					}
					brAddr = brAddr->Next;
				}
			}
			if (valid_ip) {
				ip = local_addr;
				braddr = br_addr;
			}
			CurAddrs = CurAddrs->Next;
		}
	}

	return 0;
}