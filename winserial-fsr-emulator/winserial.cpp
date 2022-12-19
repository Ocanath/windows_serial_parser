#include "winserial.h"

int connect_to_usb_serial(HANDLE* serial_handle, const char* com_port_name, unsigned long baud)
{
	/*First, connect to com port.
	TODO: add a method that scans  this and filters based on the device descriptor.
	*/
	(*serial_handle) = CreateFileA(com_port_name, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	DCB serial_params = { 0 };
	serial_params.DCBlength = sizeof(serial_params);
	serial_params.BaudRate = baud;
	serial_params.ByteSize = DATABITS_8;
	serial_params.StopBits = ONESTOPBIT;
	serial_params.Parity = PARITY_NONE;
	if (!SetCommState((*serial_handle), &serial_params))
	{
		return 1;	//fail
	}
	else
	{
		return 0;	//success
	}
}
