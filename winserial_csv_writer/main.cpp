#include<stdio.h>
#include "winserial.h"
#include <stdint.h>
#include <fstream>
#include <iostream>
#include "SerialReader.h"

uint8_t rx_buf[4096];	//oversize buffer for stack speed. divide this by 8 to find the max num words possible to transmit by the transmitter

int main()
{
	int num_words_per_packet = 6;
	int circular_buffer_size = 2*3700;

	SerialReader sr(num_words_per_packet, circular_buffer_size);
	int connected = sr.connect("\\\\.\\COM4", 921600);
	if(connected)
	{
		while (sr.is_full() == 0)
		{
			sr.do_readloop();
		}
		//sr.offload_i32("log_int.csv");
		sr.offload_f32("log_float.csv");
	}
}