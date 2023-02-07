#include<stdio.h>
#include "winserial.h"
#include <stdint.h>
#include <fstream>
#include <iostream>


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
	u32_fmt_t* d;
}data32_t;

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

uint32_t get_fletchers_checksum32(uint32_t* arr, int size, uint32_t* chk1);
void add_circ_buffer_element(uint8_t* new_data, int data32_num_words, circ_buffer_t* cb);
int start_idx_of_checksum_packet(uint8_t* rx_buf, int buf_size, int num_words_packet);
uint8_t checksum_matches(uint32_t* pData, int num_words);
void offload_circ_buffer_to_csv(std::ofstream& fs, int num_words_per_entry, circ_buffer_t* cb);


class SerialReader
{
public:
	//create log and csvbuffer arrays
	circ_buffer_t cb;
	uint8_t rx_buf[4096];	//oversize buffer for stack speed. divide this by 8 to find the max num words possible to transmit by the transmitter
	int read_size;
	data32_t part;
	HANDLE usb_serial_port;
	int num_words_per_packet;
	void (* new_pkt_callback)(uint8_t * data, int num_bytes);

	SerialReader(int nwords_per_pkt, int circular_buffer_size)
	{
		new_pkt_callback = NULL;
		num_words_per_packet = nwords_per_pkt;
		cb.size = circular_buffer_size;
		cb.read_idx = 0;
		cb.read_size = 0;
		cb.write_idx = 0;
		cb.full = 0;
		cb.buf = new data32_t[cb.size];
		for (int i = 0; i < cb.size; i++)
		{
			cb.buf[i].d = new u32_fmt_t[num_words_per_packet];
		}
		part.d = new u32_fmt_t[num_words_per_packet];

		read_size = (sizeof(u32_fmt_t) * num_words_per_packet) * 2;		//DOUBLE BUFFER!
		if (read_size > sizeof(rx_buf))
		{
			printf("Error: requested size too large\r\n");
			//return -1;
		}
	}

	~SerialReader()
	{
		for (int i = 0; i < cb.size; i++)
		{
			delete[] cb.buf[i].d;
		}
		delete[] cb.buf;
		delete[] part.d;
	}

	int connect(const char * port, int baud)
	{
		if (connect_to_usb_serial(&usb_serial_port, port, baud) != 0)
		{
			printf("found com port success\r\n");
			return 1;
		}
		else
		{
			printf("failed to find com port\r\n");
			return 0;
		}
	}

	void do_readloop(void)
	{
		//ReadFileEx(usb_serial_port, )	//note: this completes with a callback. TODO: use it instead of the blocking version!
		//requires some RTFD, however. gonna get the dumb method working first

		uint32_t num_bytes_read = 0;
		int rc = ReadFile(usb_serial_port, rx_buf, read_size, (LPDWORD)(&num_bytes_read), NULL);	//should be a DOUBLE BUFFER!

		//search the double buffer for a matching checksum
		if (num_bytes_read == read_size)
		{
			//scan array for a collection of bytes of expected size with matching checksum, and load that index into startidx
			//todo: encapsulate this as a function and unit test if things aren't working
			//NOTE: this uses FLETCHERS checksum, which uses sum of the checksum updates to evaluate a lightweight, order-sensitive checksum
			int startidx = start_idx_of_checksum_packet(rx_buf, num_bytes_read, num_words_per_packet);

			//if you have found a fully formed packet in the soup of garbage you just read
			if (startidx >= 0)
			{
				//log the first legit data found, no matter what. non-negative startidx indicates this payload is valid, so we don't have to do additional checks;
				add_circ_buffer_element((&rx_buf[startidx]), num_words_per_packet, &cb);
				if (new_pkt_callback != NULL)
				{
					(*new_pkt_callback)(&rx_buf[startidx], num_words_per_packet * sizeof(u32_fmt_t));
				}

				if (startidx != 0)	//if startidx is not at the beginning, try to realign things with another small read call
				{
					int cpy_start = startidx + num_words_per_packet * sizeof(u32_fmt_t);
					uint8_t* arr8 = (uint8_t*)(&part.d[0]);
					for (int i = cpy_start; i < sizeof(rx_buf); i++)
					{
						arr8[i - cpy_start] = rx_buf[i];
					}
					int nbytes_to_read = startidx;
					int rc = ReadFile(usb_serial_port, &(arr8[sizeof(rx_buf) - cpy_start]), nbytes_to_read, (LPDWORD)(&num_bytes_read), NULL);

					/*log the remaining part into the csvbuffer*/
					if (checksum_matches((uint32_t*)(&part.d), num_words_per_packet))	//must verify the match to log here. it's possible part contains a spurious byte!
					{
						add_circ_buffer_element((uint8_t*)(&part.d[0]), num_words_per_packet, &cb);
						if (new_pkt_callback != NULL)
						{
							(*new_pkt_callback)((uint8_t*)(&part.d[0]), num_words_per_packet * sizeof(u32_fmt_t));
						}
					}
				}
				else	//if the second half of the buffer matches, log it also. so far ONLY part that has been checked is the FIRST HALF, so we gotta scan this half too and make sure it's valid
				{
					uint8_t* pdata = &rx_buf[startidx + num_words_per_packet * sizeof(u32_fmt_t)];
					if (checksum_matches((uint32_t*)pdata, num_words_per_packet))
					{
						add_circ_buffer_element(pdata, num_words_per_packet, &cb);
						if (new_pkt_callback != NULL)
						{
							(*new_pkt_callback)(pdata, num_words_per_packet * sizeof(u32_fmt_t));
						}
					}
				}
			}
		}
	}

	void offload_i32(const char * name)
	{
		std::ofstream fs(name);

		for (int w = 0; w < num_words_per_packet; w++)
		{
			for (int i = 0; i < cb.read_size; i++)
			{
				int bidx = (i + cb.read_idx) % cb.size;
				if (i < (cb.read_size - 1))
				{
					fs << cb.buf[bidx].d[w].i32 << ',';
				}
				else
				{
					fs << cb.buf[bidx].d[w].i32;
				}
			}
			fs << '\n';
		}
		fs.close();
	}

	void offload_f32(const char* name)
	{
		std::ofstream fs(name);

		for (int w = 0; w < num_words_per_packet; w++)
		{
			for (int i = 0; i < cb.read_size; i++)
			{
				int bidx = (i + cb.read_idx) % cb.size;
				if (i < (cb.read_size - 1))
				{
					fs << cb.buf[bidx].d[w].f32 << ',';
				}
				else
				{
					fs << cb.buf[bidx].d[w].f32;
				}
			}
			fs << '\n';
		}
		fs.close();
	}

	int is_full(void) 
	{
		return (cb.full);
	}

};