#ifndef WINUDPCLIENT_H
#define	WINUDPCLIENT_H	
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include<stdio.h>
#include<winsock2.h>
#include <WS2tcpip.h>
#include <stdint.h>

typedef union u32_fmt_t
{
	uint32_t u32;
	int32_t i32;
	float f32;
	int16_t i16[sizeof(uint32_t) / sizeof(int16_t)];
	uint16_t ui16[sizeof(uint32_t) / sizeof(uint16_t)];
	int8_t i8[sizeof(uint32_t) / sizeof(int8_t)];
	uint8_t ui8[sizeof(uint32_t) / sizeof(uint8_t)];
}u32_fmt_t;



class WinUdpClient
{
public:
	struct sockaddr_in si_other;
	struct sockaddr_in si_us;
	int s;
	int slen;
	WSADATA wsa;
	WinUdpClient(uint16_t port)
	{
		slen = sizeof(si_other);
		//Initialise winsock
		printf("\nInitialising Winsock...");
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		{
			printf("Failed. Error Code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}
		printf("Initialised.\n");
		//create socket
		if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
		{
			printf("socket() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}


		memset((char*)&si_us, 0, sizeof(si_us));
		si_us.sin_family = AF_INET;

		//setup address structure
		memset((char*)&si_other, 0, sizeof(si_other));
		si_other.sin_family = AF_INET;
		si_other.sin_port = htons(port);
		//si_other.sin_addr.S_un.S_addr = inet_addr(inet_addr_buf); //assign the client the last IP address in the IPV4 list provided by the host name address list lookup. 
		//si_other.sin_addr.S_un.S_addr = inet_addr("192.168.56.255");
		si_other.sin_addr.S_un.S_addr = inet_addr("127.0.0.0");
		//si_other.sin_addr.S_un.S_addr = inet_addr("98.148.246.163");
		//si_other.sin_addr = in4addr_any;
		
		const char inet_addr_buf[256] = { 0 };
		inet_ntop(AF_INET, &si_other.sin_addr.S_un.S_addr, (PSTR)inet_addr_buf, 256);	//convert again the value we copied thru and display
		//printf("Target address: %s on port %d\r\n", inet_addr_buf, port);

	}
	int set_nonblocking(void)
	{
		//set to blocking and clear
		u_long iMode = 1;
		return ioctlsocket(s, FIONBIO, &iMode);
	}
	ULONG get_bkst_ip(void)
	{
		char namebuf[256] = { 0 };
		int rc = gethostname(namebuf, 256);
		printf("hostname: %s\r\n", namebuf);
		hostent* phost = gethostbyname(namebuf);
		struct in_addr address;
		u32_fmt_t fmt;
		for (int i = 0; phost->h_addr_list[i] != NULL; i++)
		{
			memcpy(&address, phost->h_addr_list[i], sizeof(struct in_addr));

			fmt.u32 = (uint32_t)(address.S_un.S_addr);
			printf("Host IP: %d.%d.%d.%d\r\n", fmt.ui8[0], fmt.ui8[1], fmt.ui8[2], fmt.ui8[3]);
		}
		fmt.ui8[3] = 0xFF;
		printf("Will use IP: %d.%d.%d.%d\r\n", fmt.ui8[0], fmt.ui8[1], fmt.ui8[2], fmt.ui8[3]);

		return fmt.u32;
	}
	~WinUdpClient()
	{

	}
};



#endif
