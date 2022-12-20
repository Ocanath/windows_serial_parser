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


#define NUM_32BIT_WORDS	3	//number of words per transmission, INCLUDING the checksum appended to the end of the message!
#define CSVBUFFER_SIZE	10

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



uint8_t rx_buf[sizeof(data32_t)*2];	//double buffer

int main()
{
	HANDLE usb_serial_port;
	if (connect_to_usb_serial(&usb_serial_port, "\\\\.\\COM4", 921600) != 0)
		printf("found com port success\r\n");
	else
		printf("failed to find com port\r\n");

	//create log and csvbuffer arrays
	data32_t* csvbuffer = new data32_t[CSVBUFFER_SIZE];	//put this on the heap for speed
	int* log = new int[CSVBUFFER_SIZE];

	//init event log and csvbuffer arrays
	for (int i = 0; i < CSVBUFFER_SIZE; i++)
	{
		log[i] = 0;
		for(int w = 0; w < NUM_32BIT_WORDS; w++)
			csvbuffer[i].d[w].u32 = 0xDEADBEEF;
	}

	int csvbuffer_idx = 0;

	uint64_t start_tick_64 = GetTickCount64();
	uint32_t mismatch_count = 0;
	data32_t part = { 0 };
	while (1)
	{
		//ReadFileEx(usb_serial_port, )	//note: this completes with a callback. TODO: use it instead of the blocking version!
		//requires some RTFD, however. gonna get the dumb method working first

		uint32_t num_bytes_read = 0;
		int rc = ReadFile(usb_serial_port, rx_buf, sizeof(rx_buf), (LPDWORD)(&num_bytes_read), NULL);
		
		//search the double buffer for a matching checksum
		if(num_bytes_read == sizeof(rx_buf))
		{
			//scan array for a collection of bytes of expected size with matching checksum, and load that index into startidx
			//todo: encapsulate this as a function and unit test if things aren't working
			int startidx = 0;
			for (int s = 0; s < (num_bytes_read/2); s++)
			{
				uint32_t* arr32 = (uint32_t*)(&rx_buf[s]);
				uint32_t chk = get_checksum32(arr32, NUM_32BIT_WORDS - 1);
				
				if (chk == arr32[NUM_32BIT_WORDS - 1])
				{
					startidx = s;					
					break;
				}
			}

			//log the part of the buffer beggining at the located start index which has a matching checksum
			if (csvbuffer_idx < CSVBUFFER_SIZE)
			{
				memcpy(&csvbuffer[csvbuffer_idx], &rx_buf[startidx], sizeof(data32_t));
				log[csvbuffer_idx] = 1;
				csvbuffer_idx++;
			}

			//if you have an alignment issue, attempt to resolve it by loading the remainder of the bytes not stored in the double buffer of the next part. we discard the first part always
			if (startidx != 0)
			{
				int cpy_start = startidx + sizeof(data32_t);
				uint8_t* arr8 = (uint8_t*)(&part);
				for (int i = cpy_start; i < sizeof(rx_buf); i++)
				{
					arr8[i - cpy_start] = rx_buf[i];
				}
				int nbytes_to_read = startidx;
				int rc = ReadFile(usb_serial_port, &(arr8[sizeof(rx_buf) - cpy_start]), nbytes_to_read, (LPDWORD)(&num_bytes_read), NULL);
				
				/*log the remaining part into the csvbuffer*/
				if (csvbuffer_idx < CSVBUFFER_SIZE)
				{
					memcpy(&csvbuffer[csvbuffer_idx], &part, sizeof(data32_t));
					log[csvbuffer_idx] = 3;
					csvbuffer_idx++;
				}
			}
			else	//log the second half of the buffer as well, if start idx is zero
			{
				if (csvbuffer_idx < CSVBUFFER_SIZE)
				{
					memcpy(&csvbuffer[csvbuffer_idx], &rx_buf[startidx+sizeof(data32_t)], sizeof(data32_t));
					log[csvbuffer_idx] = 2;
					csvbuffer_idx++;
				}
			}

		}
		if (csvbuffer_idx >= CSVBUFFER_SIZE)
			break;
	}
	printf("scanning csv:\r\n");
	for (int i = 0; i < CSVBUFFER_SIZE; i++)
	{
		printf("0x");
		for (int w = 0; w < NUM_32BIT_WORDS; w++)
		{
			printf("%0.2X", csvbuffer[i].d[w].u32);
		}
		printf("\r\n");
		uint32_t chk = get_checksum32((uint32_t*)(&csvbuffer[i]), NUM_32BIT_WORDS - 1);
		if (chk == csvbuffer[i].d[NUM_32BIT_WORDS - 1].u32)
		{
			printf(", pass");
		}
		else
		{
			printf(", fail");
		}
		printf(", logevt %d\r\n", log[i]);
	}

	printf("writing csv...\r\n");

	delete[] csvbuffer;
	delete[] log;
}