#include<stdio.h>
#include "winserial.h"
#include <stdint.h>
#include <fstream>
#include <iostream>

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


#define NUM_32BIT_WORDS	6	//number of words per transmission, INCLUDING the checksum appended to the end of the message!

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
uint32_t get_fletchers_checksum32(uint32_t* arr, int size, uint32_t * chk1)
{
	int32_t checksum = 0;
	int32_t fchk = 0;
	for (int i = 0; i < size; i++)
	{
		checksum += (int32_t)arr[i];
		fchk += checksum;
	}
	if(chk1 != NULL)
		*chk1 = (uint32_t)(-checksum);
	return fchk;
}


/**/
uint8_t checksum_matches(data32_t* pData)
{
	int num_words = sizeof(data32_t) / sizeof(u32_fmt_t);
	uint32_t* p32 = (uint32_t*)pData;
	if (p32[num_words - 1] == get_fletchers_checksum32(p32, num_words - 1, NULL))
		return 1;
	else
		return 0;
}


int start_idx_of_checksum_packet(uint8_t* rx_buf, int buf_size, int num_words_packet)
{
	int startidx = -1;
	for (int s = 0; s < (buf_size / 2); s++)
	{
		uint32_t* arr32 = (uint32_t*)(&rx_buf[s]);
		uint32_t chk = get_fletchers_checksum32(arr32, num_words_packet - 1, NULL);

		if (chk == arr32[num_words_packet - 1])
		{
			startidx = s;
			return startidx;
		}
	}
	return startidx;
}


/*
* Init to:
* start_idx = 1
* end_idx = 0
*
*/
typedef struct circ_buffer_t
{
	data32_t* buf;
	int write_idx;
	int read_idx;
	int read_size;
	int size;
	uint8_t full;
}circ_buffer_t;

/*could generalize data32 to a void * and entry size, but imma keep it as is for now*/
void add_circ_buffer_element(data32_t* new_entry, circ_buffer_t* cb)
{
	memcpy(&cb->buf[cb->write_idx], new_entry, sizeof(data32_t));
	cb->write_idx = cb->write_idx + 1;

	if (cb->full == 0)
	{
		cb->read_idx = 0;
		cb->read_size++;
	}
	else
	{
		cb->read_idx = cb->write_idx;
	}

	if (cb->write_idx >= cb->size)
	{
		cb->write_idx = 0;
		cb->full = 1;
	}
}

void offload_circ_buffer_to_csv(std::ofstream &fs, circ_buffer_t* cb)
{
	for (int w = 0; w < NUM_32BIT_WORDS; w++)
	{
		for (int i = 0; i < cb->read_size; i++)
		{
			int bidx = (i + cb->read_idx) % cb->size;
			if (i < (cb->read_size - 1))
			{
				fs << cb->buf[bidx].d[w].i32 << ',';

			}
			else
			{
				fs << cb->buf[bidx].d[w].i32;
			}
		}
		fs << '\n';
	}
}

uint8_t rx_buf[sizeof(data32_t)*2];	//double buffer

int main()
{
	HANDLE usb_serial_port;
	if (connect_to_usb_serial(&usb_serial_port, "\\\\.\\COM4", 921600) != 0)
		printf("found com port success\r\n");
	else
		printf("failed to find com port\r\n");

	std::ofstream fs("log.csv");

	int buffer_size = 10;



	//create log and csvbuffer arrays
	circ_buffer_t cb;
	cb.size = buffer_size;
	cb.read_idx = 0;
	cb.read_size = 0;
	cb.write_idx = 0;
	cb.full = 0;
	cb.buf = new data32_t[cb.size];

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
			//NOTE: this uses FLETCHERS checksum, which uses sum of the checksum updates to evaluate a lightweight, order-sensitive checksum
			int startidx = start_idx_of_checksum_packet(rx_buf, num_bytes_read, NUM_32BIT_WORDS);

			//if you have found a fully formed packet in the soup of garbage you just read
			if (startidx >= 0)
			{
				//log the first legit data found, no matter what. non-negative startidx indicates this payload is valid, so we don't have to do additional checks;
				add_circ_buffer_element((data32_t*)(&rx_buf[startidx]), &cb);

				if (startidx != 0)	//if startidx is not at the beginning, try to realign things with another small read call
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
					if (checksum_matches(&part))	//must verify the match to log here. it's possible part contains a spurious byte!
					{
						add_circ_buffer_element(&part, &cb);
					}
				}
				else	//if the second half of the buffer matches, log it also. so far ONLY part that has been checked is the FIRST HALF, so we gotta scan this half too and make sure it's valid
				{
					data32_t* pdata = (data32_t*)(&rx_buf[startidx + sizeof(data32_t)]);
					if (checksum_matches(pdata))
					{
						add_circ_buffer_element(pdata, &cb);
					}
				}
			}
		}
		if (cb.full)
			break;
	}

	offload_circ_buffer_to_csv(fs, &cb);
	//printf("scanning csv:\r\n");
	//for (int i = 0; i < CSVBUFFER_SIZE; i++)
	//{
	//	printf("0x");
	//	for (int w = 0; w < NUM_32BIT_WORDS; w++)
	//	{
	//		printf("%0.8X", csvbuffer[i].d[w].u32);
	//	}
	//	uint32_t chk = get_checksum32((uint32_t*)(&csvbuffer[i]), NUM_32BIT_WORDS - 1);
	//	if (chk == csvbuffer[i].d[NUM_32BIT_WORDS - 1].u32)
	//	{
	//		printf(", pass");
	//	}
	//	else
	//	{
	//		printf(", fail");
	//	}
	//	printf(", logevt %d\r\n", log[i]);
	//}

	//printf("writing csv...\r\n");

	//for (int row = 0; row < NUM_32BIT_WORDS - 1; row++)
	//{
	//	for (int col = 0; col < CSVBUFFER_SIZE; col++)
	//	{
	//		if (checksum_matches(&csvbuffer[row]))
	//		{
	//			fs << csvbuffer[col].d[row].i32;
	//			if (col < (CSVBUFFER_SIZE - 1))
	//			{
	//				fs << ",";
	//			}
	//		}
	//	}
	//	fs << "\n";
	//}

	fs.close();
	delete[] cb.buf;
}