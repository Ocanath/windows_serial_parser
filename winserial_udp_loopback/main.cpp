#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include<winsock2.h>
#include <WS2tcpip.h>
#include "WinUdpClient.h"
#include  "SerialReader.h"
#include "winserial.h"

#pragma comment(lib,"ws2_32.lib") //Winsock Library

void* p_udpclient = NULL;


void callback(uint8_t* data, int num_bytes)
{
	int num_words = (num_bytes / sizeof(u32_fmt_t)) - 1;	//discard the fletcher's checksum in parsing
	u32_fmt_t* pfmt = (u32_fmt_t*)data;

	printf("i32={");
	for (int i = 0; i < num_words; i++)
		printf("%d, ", pfmt[i].i32);
	printf("}, f32={");
	//for (int i = 0; i < num_words; i++)
	//	printf("%f, ", pfmt[i].f32);
	//printf("}\r\n");

	
	//WinUdpClient* pcli = (WinUdpClient*)p_udpclient;
	//sendto(pcli->s, (const char *)data, num_bytes, 0, (struct sockaddr*)(&pcli->si_other), pcli->slen);
	float pf[5];
	pf[0] = ((float)pfmt[0].i32) / 1000.f;
	for (int i = 1; i < num_words; i++)
	{
		pf[i] = ((float)pfmt[i].i32) / (3.141592f * 4096.f * 649);
	}
	for (int i = 0; i < num_words; i++)
	{
		printf("%f, ", pf[i]);
	}
	printf("\r\n");
	WinUdpClient* pcli = (WinUdpClient*)p_udpclient;
	sendto(pcli->s, (const char *)(&pf), num_words*sizeof(float), 0, (struct sockaddr*)(&pcli->si_other), pcli->slen);


}


int main()
{
	WinUdpClient client(4537);
	client.set_nonblocking();
	p_udpclient = (void*)(&client);

	char inet_addr_buf[256] = { 0 };
	client.si_other.sin_addr.S_un.S_addr = 1 << 24 | 0 << 16 | 0 << 8 | 127;//client.get_bkst_ip();
	inet_ntop(AF_INET, &client.si_other.sin_addr.S_un.S_addr, (PSTR)inet_addr_buf, 256);	//convert again the value we copied thru and display
	printf("Target address: %s on port %d\r\n", inet_addr_buf, client.si_other.sin_port);


	int num_words_per_packet = 6;
	int circular_buffer_size = 3700;
	
	SerialReader sr(num_words_per_packet, circular_buffer_size);
	int connected = sr.connect("\\\\.\\COM3", 460800);
	sr.new_pkt_callback = &callback;

	if (connected)
	{
		//while (sr.is_full() == 0)
		while(1)
		{
			sr.do_readloop();
		}
		//sr.offload_i32("log_int.csv");
		//sr.offload_f32("log_float.csv");
	}


}
