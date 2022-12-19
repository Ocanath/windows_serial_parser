#include<stdio.h>
#include "winserial.h"
#include <stdint.h>
/*
* A note on this application.
* 
*/


#define NUM_32BIT_WORDS	3

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

uint8_t get_checksum(uint8_t* arr, int size)
{

	int8_t checksum = 0;
	for (int i = 0; i < size; i++)
		checksum += (int8_t)arr[i];
	return -checksum;
}

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



static const int frame_size = sizeof(u32_fmt_t) * NUM_32BIT_WORDS;	//2 words payload, 1 word checksum
uint8_t rx_buf[frame_size];
u32_fmt_t* fmt_buf = (u32_fmt_t*)(&rx_buf);	//this WILL segfault

int main()
{
	HANDLE usb_serial_port;
	if (connect_to_usb_serial(&usb_serial_port, "\\\\.\\COM4", 921600, frame_size) != 0)
		printf("found com port success\r\n");
	else
		printf("failed to find com port\r\n");


	uint64_t start_tick_64 = GetTickCount64();

	while (1)
	{
		ReadFileEx(usb_serial_port, )


	}
}