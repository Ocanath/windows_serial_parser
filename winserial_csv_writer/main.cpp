#include<stdio.h>
#include "winserial.h"
#include <stdint.h>
/*
* A note on this application.
* Windows supports a total frame timeout in ms
* I do not know how much jitter there is in this timeout.
* 
* 
* However, assuming reasonable accuracy, it should be possible to
* set a total frame timeout value that is quite close to the actual frame duration
* 
* For instance. An 88 byte buffer is the largest multiple of 4 buffer size which is
* smaller than 1ms. the duration remaining after this frame is a little bit more than 1ms
* 
* SO it should be fine to concatenate multiple sections of data together with a timestamp, and then
* send it all over in one frame for highest efficiency. For instance a 21 word buffer, with 22nd word as
* checksum, you can load 7 vectors of 32bit words (x,y,t), for a theoretical bandwidth of 88bytes per millisecond,
* pretty close to continuously using the max baud!
* 
* 
* UPDATE: appears to only work if intervaltimeout is nonzero. hard set it to 1 and it should be ok
*/


#define NUM_32BIT_WORDS	2	//number of words per transmission, NOT INCLUDING the checksum appended to the end of the message!
#define CSVBUFFER_SIZE	1024


#define MAX_RX_BUF_SIZE	128	//larger than actually used

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

typedef struct data32_t
{
	u32_fmt_t d[NUM_32BIT_WORDS];
}data32_t;


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



static const int frame_size = sizeof(u32_fmt_t) * (NUM_32BIT_WORDS + 1);	//2 words payload, 1 word checksum
uint8_t rx_buf[MAX_RX_BUF_SIZE];
u32_fmt_t* fmt_buf = (u32_fmt_t*)(&rx_buf);	//ensure MAX_RX_BUF_SIZE is a multiple of 4!

int main()
{
	HANDLE usb_serial_port;
	if (connect_to_usb_serial(&usb_serial_port, "\\\\.\\COM4", 921600, frame_size) != 0)
		printf("found com port success\r\n");
	else
		printf("failed to find com port\r\n");

	data32_t* csvbuffer = new data32_t[CSVBUFFER_SIZE];	//put this on the heap for speed
	int csvbuffer_idx = 0;

	uint64_t start_tick_64 = GetTickCount64();

	while (1)
	{
		//ReadFileEx(usb_serial_port, )	//note: this completes with a callback. TODO: use it instead of the blocking version!
		//requires some RTFD, however. gonna get the dumb method working first

		uint32_t num_bytes_read=0;
		int rc = ReadFile(usb_serial_port, rx_buf, sizeof(rx_buf), (LPDWORD)(&num_bytes_read), NULL);

		if(num_bytes_read > 0)	//rc always 0 here because we are intentionally forcing error state every single time! 1337 haxxor *dons sunglasses*
		{
			DWORD error = GetLastError();
			uint8_t chkmatch = 0;
			//if((num_bytes_read % sizeof(u32_fmt_t)) == 0)
			if(num_bytes_read == (sizeof(data32_t)+sizeof(u32_fmt_t)) )	//check for specific size
			{
				int num_words32 = num_bytes_read / sizeof(u32_fmt_t);
				uint32_t chksm32 = get_checksum32((uint32_t*)rx_buf, num_words32-1);
				if (chksm32 == fmt_buf[num_words32 - 1].u32)
				{
					chkmatch = 1;
					memcpy(&csvbuffer[csvbuffer_idx].d[0].ui8[0], &(rx_buf[0]), sizeof(data32_t));
					csvbuffer_idx++;
					if (csvbuffer_idx > CSVBUFFER_SIZE)
					{
						break;
					}
				}
			}
			//else if (chkmatch == 0)
			{
				printf("error code %d, received %lu bytes, match = %d, buf = 0x", error, num_bytes_read, chkmatch);
				for (int i = 0; i < num_bytes_read; i++)
				{
					printf("%0.2X", rx_buf[i]);
				}
				printf("\r\n");
			}
		}
	}
	printf("writing csv...\r\n");

	delete[] csvbuffer;
}