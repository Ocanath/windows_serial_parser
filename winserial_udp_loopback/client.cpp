/*
	Simple udp client
*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include<stdio.h>
#include<winsock2.h>
#include <WS2tcpip.h>
#include "sin_math.h"
#include "WinUdpClient.h"
#include <intrin.h>


#pragma comment(lib,"ws2_32.lib") //Winsock Library

#include <iostream>

#define BUFLEN 512	//Max length of buffer
#define PORT 34345	//The port on which to listen for incoming data


/*
Generic hex checksum calculation.
TODO: use this in the psyonic API
*/
uint32_t get_checksum32(uint32_t* arr, int size)
{
	int32_t checksum = 0;
	for (int i = 0; i < size; i++)
		checksum += (int32_t)arr[i];
	return -checksum;
}


/*
Generic hex checksum calculation.
TODO: use this in the psyonic API
*/
uint32_t fletchers_checksum32(uint32_t* arr, int size)
{
	int32_t checksum = 0;
	int32_t fchk = 0;
	for (int i = 0; i < size; i++)
	{
		checksum += (int32_t)arr[i];
		fchk += checksum;
	}
	return fchk;
}

void ipedit(char* buf)
{
	int num_periods = 0;
	int i;
	for (i = 0; num_periods < 3; i++)
	{
		if (buf[i] == '.')
			num_periods++;
	}
	buf[i] = '2';
	buf[i + 1] = '5';
	buf[i + 2] = '5';
	buf[i + 3] = '\0';
}


int main(void)
{
	uint8_t buf[1024];
	WinUdpClient client(PORT);
	client.set_nonblocking();
	
	//192.168.29.199
	char inet_addr_buf[256] = { 0 };
	client.si_other.sin_addr.S_un.S_addr = 251 << 24 | 29 << 16 | 168 << 8 | 192;//client.get_bkst_ip();
	inet_ntop(AF_INET, &client.si_other.sin_addr.S_un.S_addr, (PSTR)inet_addr_buf, 256);	//convert again the value we copied thru and display
	printf("Target address: %s on port %d\r\n", inet_addr_buf, client.si_other.sin_port);
	
	//start communication
	u32_fmt_t farr[40] = { 0 };
	SYSTEMTIME st;
	uint32_t fail_count = 0;

	uint64_t start_tick_64 = GetTickCount64();
	uint64_t report_ts = 0;
	uint32_t uptick = 0;

	const char* sendbuf = "hello???";
	while (1)
	{
		int recieved_length = recvfrom(client.s, (char*)buf, BUFLEN, 0, (struct sockaddr*)&(client.si_other), &client.slen);
		if (sendto(client.s, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&client.si_other, client.slen) != SOCKET_ERROR)
		{
			//printf("Successsfully sent message\r\n");
			//printf("sendto() failed with error code : %d", WSAGetLastError());
			//exit(EXIT_FAILURE);
		}
	}
	while (1)
	{
		uint64_t tick = GetTickCount64() - start_tick_64;
		float t = (float)tick * .001f;

		/*create a payload*/
		for (int i = 0; i < 6; i++)
		{
			farr[i].f32 = 30.f*(cos_fast(wrap_2pi(t) + (float)i / TWO_PI) + 1);	//for later
		}
		farr[5].f32 = -farr[5].f32;
		farr[6].u32 = get_checksum32( (uint32_t*)(&(farr[0].u32)), 6);

		//sprintf(t_buf, "client uptime: %f\r\n", t);


		//blindly attempt sending hello to our server so if it restarts it can find us
		if(tick > report_ts)
		{
			report_ts = tick + 750;	//send udp packet once every 50 milliseconds (or so)
			sendto(client.s, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&client.si_other, client.slen);
		}
		
		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', BUFLEN);
		//try to receive some data, this is a blocking call			
		int recieved_length = recvfrom(client.s, (char*)buf, BUFLEN, 0, (struct sockaddr*)&(client.si_other), &client.slen);
		if(recieved_length > 0)
		{
			if ((recieved_length % 4) == 0)
			{
				printf("recieved %d total bytes: ", recieved_length);
				for (int i = 0; i < recieved_length/4; i++)
				{
					//printf("0x%.8X ", ((int32_t*)buf)[i]);
					printf("%d ", ((int32_t*)buf)[i]);
				}
				printf("\r\n");
			}
		}
		
	}

	closesocket(client.s);
	WSACleanup();

	return 0;
}