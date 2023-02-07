#include "SerialReader.h"

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
uint32_t get_fletchers_checksum32(uint32_t* arr, int size, uint32_t* chk1)
{
	int32_t checksum = 0;
	int32_t fchk = 0;
	for (int i = 0; i < size; i++)
	{
		checksum += (int32_t)arr[i];
		fchk += checksum;
	}
	if (chk1 != NULL)
		*chk1 = (uint32_t)(-checksum);
	return fchk;
}


/**/
uint8_t checksum_matches(uint32_t* pData, int num_words)
{
	if (pData[num_words - 1] == get_fletchers_checksum32(pData, num_words - 1, NULL))
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




/*could generalize data32 to a void * and entry size, but imma keep it as is for now*/
void add_circ_buffer_element(uint8_t* new_data, int data32_num_words, circ_buffer_t* cb)
{
	memcpy(&cb->buf[cb->write_idx].d[0], new_data, data32_num_words * sizeof(u32_fmt_t));
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
