#include<stdio.h>
#include "winserial.h"
#include <stdint.h>

/*
 * This function takes in an array of 12 bit values (stored in uint16_t containers)
 * and packs them into an 8 bit array.
*/
void pack_12bit_into_8bit(uint16_t* vals, int valsize, uint8_t* arr)
{
	for (int bidx = valsize * 12 - 4; bidx >= 0; bidx -= 4)
	{
		int validx = bidx / 12;
		int arridx = bidx / 8;
		int shift_val = (bidx % 12);
		arr[arridx] |= ((vals[validx] >> shift_val) & 0x0F) << (bidx % 8);
	}
}

uint8_t get_checksum(uint8_t* arr, int size)
{

	int8_t checksum = 0;
	for (int i = 0; i < size; i++)
		checksum += (int8_t)arr[i];
	return -checksum;
}

int main()
{
	HANDLE usb_serial_port;
	if (connect_to_usb_serial(&usb_serial_port, "\\\\.\\COM4", 230400) == 0)
		printf("found com port success\r\n");
	else
		printf("failed to find com port\r\n");

	uint64_t start_tick_64 = GetTickCount64();
	uint64_t blast_ts = 0;

	uint64_t vib_ts = 0;

	uint16_t val_arr[6] = { 0 };
	uint8_t tx_arr[512] = { 0 };
	enum {TRIGGER_VIB, CLEAR_VIB};
	int state = CLEAR_VIB;
	while (1)
	{
		uint64_t tick = GetTickCount64();

		if (tick > vib_ts)
		{
			switch (state)
			{
				case TRIGGER_VIB:
				{
					val_arr[0] = 2000;
					vib_ts = tick + 500;
					state = CLEAR_VIB;
					break;
				}
				case CLEAR_VIB:
				{
					val_arr[0] = 1;
					vib_ts = tick + 500;
					state = TRIGGER_VIB;
					break;
				}
			};			
		}


		if (tick > blast_ts)	//imma blast em
		{
			//blast_ts = tick + 15;
			
			int len = 1;
			tx_arr[0] = 0x7F;
			//int len = rand() % 255;
			//for (int i = 0; i < len; i++)
			//	tx_arr[i] = rand();

			//for (int i = 0; i < 10; i++)
			//	tx_arr[i] = 0;
			//pack_12bit_into_8bit(val_arr, 6, tx_arr);
			//tx_arr[9] = get_checksum(tx_arr, 9);

			printf("buf: 0x");
			for (int i = 0; i < len; i++)
			{
				printf("%0.2X", tx_arr[i]);
			}
			printf("\r\n");

			DWORD dwbyteswritten;
			WriteFile(usb_serial_port, tx_arr, len, &dwbyteswritten, NULL);
		}

	}
}