#ifndef WINSERIAL_H
#define WINSERIAL_H

#include <Windows.h>

int connect_to_usb_serial(HANDLE* serial_handle, const char* com_port_name, unsigned long baud);

#endif