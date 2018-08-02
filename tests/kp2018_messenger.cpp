// serial tdma test is simple test program for the tdma stuff
// use arguments "-p "serial port name" -i" to get connection info
// use arguments "-p "serial port name" -r" to receive a message
// use arguments "-p "serial port name" -a FF -s "message"" to send a message

#include <Windows.h>

BOOL search_argument(SIZE_T argument_count, const WCHAR** argument_values, const WCHAR* short_argument, const WCHAR* long_argument, const WCHAR** value)
{
	for (SIZE_T i = 0; i != argument_count; ++i)
		if ((short_argument && !lstrcmpW(short_argument, argument_values[i])) || (long_argument && !lstrcmpW(long_argument, argument_values[i])))
		{
			if (value)
				*value = i + 1 != argument_count ? argument_values[i + 1] : 0;
			return TRUE;
		}
	if (value)
		*value = 0;
	return FALSE;
}

DWORD get_process_arguments(HANDLE heap, SIZE_T* argument_count, WCHAR*** argument_values)
{
	DWORD error;
	HMODULE shell32 = LoadLibraryW(L"Shell32.dll");
	if (!shell32)
		return GetLastError();
	SIZE_T local_argument_count = 0;
	WCHAR** local_argument_values = ((WCHAR** (WINAPI*)(const WCHAR*, int*))GetProcAddress(shell32, "CommandLineToArgvW"))(GetCommandLineW(), (int*)&local_argument_count);
	if (!local_argument_values)
	{
		error = GetLastError();
		FreeLibrary(shell32);
		return error;
	}
	SIZE_T argument_value_data_size = 0;
	for (SIZE_T i = 0; i != local_argument_count; ++i)
		argument_value_data_size += (((SIZE_T)lstrlenW(local_argument_values[i]) + 1) * sizeof(WCHAR));
	WCHAR** argument_buffer = (WCHAR**)HeapAlloc(heap, 0, local_argument_count * sizeof(WCHAR*) + argument_value_data_size);
	if (!argument_buffer)
	{
		error = GetLastError();
		LocalFree(local_argument_values);
		FreeLibrary(shell32);
		return error;
	}
	for (SIZE_T w = local_argument_count * sizeof(WCHAR*), i = 0; i != local_argument_count; ++i)
	{
		WCHAR* p = (WCHAR*)((UINT_PTR)argument_buffer + w);
		SIZE_T s = (((SIZE_T)lstrlenW(local_argument_values[i]) + 1) * sizeof(WCHAR));
		argument_buffer[i] = p;
		for (WCHAR* copy_source = (WCHAR*)local_argument_values[i], *copy_source_end = (WCHAR*)((UINT_PTR)copy_source + s), *copy_destination = argument_buffer[i]; copy_source != copy_source_end; ++copy_source, ++copy_destination)
			*copy_destination = *copy_source;
		w += s;
	}
	LocalFree(local_argument_values);
	FreeLibrary(shell32);
	*argument_count = local_argument_count;
	*argument_values = (WCHAR**)argument_buffer;
	return 0;
}

DWORD open_serial_port(const WCHAR* serial_port_name, DWORD baud_rate, BOOL overlapped, COMMTIMEOUTS* timeouts, HANDLE* serial_port_handle)
{
	WCHAR serial_port_full_name[16];
	if (serial_port_name[0] == L'\\' && serial_port_name[1] == L'\\' && serial_port_name[2] == L'.' && serial_port_name[3] == L'\\' && serial_port_name[4] == L'C' && serial_port_name[5] == L'O' && serial_port_name[6] == L'M' && serial_port_name[7] >= L'0' && serial_port_name[7] <= L'9')
	{
		SIZE_T port_number_digit_count = 1;
		while (serial_port_name[7 + port_number_digit_count])
			if (serial_port_name[7 + port_number_digit_count] >= L'0' || serial_port_name[7 + port_number_digit_count] <= L'9')
				++port_number_digit_count;
			else
				return ERROR_INVALID_NAME;
		if (port_number_digit_count > 8)
			return ERROR_INVALID_NAME;
		for (WCHAR* r = (WCHAR*)L"\\\\.\\COM", *e = r + 7, *w = serial_port_full_name; r != e; ++r, ++w)
			*w = *r;
		for (WCHAR* r = (WCHAR*)serial_port_name + 7, *e = r + port_number_digit_count + 1, *w = serial_port_full_name + 7; r != e; ++r, ++w)
			*w = *r;
	}
	else if (serial_port_name[0] == L'C' && serial_port_name[1] == L'O' && serial_port_name[2] == L'M' && serial_port_name[3] >= L'0' && serial_port_name[3] <= L'9')
	{
		SIZE_T port_number_digit_count = 1;
		while (serial_port_name[3 + port_number_digit_count])
			if (serial_port_name[3 + port_number_digit_count] >= L'0' || serial_port_name[7 + port_number_digit_count] <= L'9')
				++port_number_digit_count;
			else
				return ERROR_INVALID_NAME;
		if (port_number_digit_count > 8)
			return ERROR_INVALID_NAME;
		for (WCHAR* r = (WCHAR*)L"\\\\.\\COM", *e = r + 7, *w = serial_port_full_name; r != e; ++r, ++w)
			*w = *r;
		for (WCHAR* r = (WCHAR*)serial_port_name + 3, *e = r + port_number_digit_count + 1, *w = serial_port_full_name + 7; r != e; ++r, ++w)
			*w = *r;
	}
	else
		return ERROR_INVALID_NAME;
	HANDLE heap = GetProcessHeap();
	if (!heap)
		return GetLastError();
	DWORD serial_configuration_size = sizeof(COMMCONFIG);
	COMMCONFIG* serial_configuration = (COMMCONFIG*)HeapAlloc(heap, 0, (SIZE_T)serial_configuration_size);
	if (!serial_configuration)
		return GetLastError();
	DWORD error;
	for (BOOL get_serial_configuration = TRUE; get_serial_configuration;)
	{
		DWORD get_serial_configuration_size = serial_configuration_size;
		if (GetDefaultCommConfigW(serial_port_full_name + 4, serial_configuration, &get_serial_configuration_size))
		{
			serial_configuration_size = get_serial_configuration_size;
			get_serial_configuration = FALSE;
		}
		else
		{
			if (get_serial_configuration_size > serial_configuration_size)
			{
				serial_configuration_size = get_serial_configuration_size;
				COMMCONFIG* new_allocation = (COMMCONFIG*)HeapReAlloc(heap, 0, serial_configuration, (SIZE_T)serial_configuration_size);
				if (!new_allocation)
				{
					error = GetLastError();
					HeapFree(heap, 0, serial_configuration);
					return error;
				}
				serial_configuration = new_allocation;
			}
			else
			{
				error = GetLastError();
				HeapFree(heap, 0, serial_configuration);
				return error;
			}
		}
	}
	serial_configuration->dcb.BaudRate = baud_rate;
	serial_configuration->dcb.ByteSize = 8;
	serial_configuration->dcb.StopBits = ONESTOPBIT;
	serial_configuration->dcb.Parity = NOPARITY;
	serial_configuration->dcb.fDtrControl = DTR_CONTROL_ENABLE;
	COMMTIMEOUTS serial_timeouts = { 0, 0, 0, 0, 0 };
	HANDLE handle = CreateFileW(serial_port_full_name, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, overlapped ? FILE_FLAG_OVERLAPPED : 0, 0);
	if (handle == INVALID_HANDLE_VALUE)
	{
		error = GetLastError();
		HeapFree(heap, 0, serial_configuration);
		return error;
	}
	if (!SetupComm(handle, 0x10000, 0x10000) || !SetCommConfig(handle, serial_configuration, serial_configuration_size) || !SetCommTimeouts(handle, timeouts ? timeouts : &serial_timeouts) || !PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR))
	{
		error = GetLastError();
		CloseHandle(handle);
		HeapFree(heap, 0, serial_configuration);
		return error;
	}
	HeapFree(heap, 0, serial_configuration);
	Sleep(0x800);
	COMSTAT serial_status;
	DWORD serial_errors;
	ClearCommError(handle, &serial_errors, &serial_status);
	*serial_port_handle = handle;
	return 0;
}

DWORD get_connection_info(HANDLE serial_port, BYTE* base_station_address, BYTE* device_address)
{
	const BYTE connection_info_identifier = 0xFF;
	DWORD error;
	DWORD io_result;
	if (!WriteFile(serial_port, &connection_info_identifier, 1, &io_result, 0) || io_result != 1)
	{
		error = GetLastError();
		return error ? error : ERROR_UNIDENTIFIED_ERROR;
	}
	BYTE configuration[2];
	for (DWORD read = 0; read != sizeof(configuration);)
		if (ReadFile(serial_port, configuration + read, sizeof(configuration) - read, &io_result, 0) && io_result)
			read += io_result;
		else
		{
			error = GetLastError();
			return error ? error : ERROR_UNIDENTIFIED_ERROR;
		}
	*base_station_address = configuration[0];
	*device_address = configuration[1];
	return 0;
}

DWORD print(HANDLE console, const WCHAR* string)
{
	if (console != INVALID_HANDLE_VALUE)
	{
		DWORD write_lenght = 0;
		DWORD string_lenght = lstrlenW(string);
		return WriteConsoleW(console, string, string_lenght, &write_lenght, 0) && write_lenght == string_lenght ? 0 : GetLastError();
	}
	return ERROR_INVALID_HANDLE;
}

#define MESSAGE_MAXIMUM_LENGTH 0xF8

DWORD receive(HANDLE serial_port, BYTE* address, SIZE_T message_buffer_size, SIZE_T* message_size, void* message_buffer)
{
	const BYTE receive_identifier = 0xFE;
	DWORD error;
	DWORD io_result;
	if (!WriteFile(serial_port, &receive_identifier, 1, &io_result, 0) || io_result != 1)
	{
		error = GetLastError();
		return error ? error : ERROR_UNIDENTIFIED_ERROR;
	}
	BYTE message_size_byte = 0;
	while (!message_size_byte)
		if (!ReadFile(serial_port, &message_size_byte, 1, &io_result, 0) || !io_result)
		{
			error = GetLastError();
			return error ? error : ERROR_UNIDENTIFIED_ERROR;
		}
	if ((SIZE_T)message_size_byte > message_buffer_size)
	{
		BYTE trash;
		for (DWORD size = (DWORD)message_size_byte + 1, read = 0; read != size;)
			if (ReadFile(serial_port, &trash, 1, &io_result, 0) && io_result)
				read += io_result;
			else
			{
				error = GetLastError();
				return error ? error : ERROR_UNIDENTIFIED_ERROR;
			}
		return ERROR_INSUFFICIENT_BUFFER;
	}
	if (!ReadFile(serial_port, address, 1, &io_result, 0) || !io_result)
	{
		error = GetLastError();
		return error ? error : ERROR_UNIDENTIFIED_ERROR;
	}
	for (DWORD size = (DWORD)message_size_byte, read = 0; read != size;)
		if (ReadFile(serial_port, (BYTE*)message_buffer + read, 1, &io_result, 0) && io_result)
			read += io_result;
		else
		{
			error = GetLastError();
			return error ? error : ERROR_UNIDENTIFIED_ERROR;
		}
	return 0;
}

DWORD send(HANDLE serial_port, BYTE address, SIZE_T message_size, const void* message)
{
	if (message_size > MESSAGE_MAXIMUM_LENGTH)
		return ERROR_INVALID_PARAMETER;
	const BYTE send_identifier = 0xFD;
	DWORD error;
	DWORD io_result;
	if (!WriteFile(serial_port, &send_identifier, 1, &io_result, 0) || io_result != 1)
	{
		error = GetLastError();
		return error ? error : ERROR_UNIDENTIFIED_ERROR;
	}
	BYTE message_size_byte = (BYTE)message_size;
	if (!WriteFile(serial_port, &message_size_byte, 1, &io_result, 0) || io_result != 1)
	{
		error = GetLastError();
		return error ? error : ERROR_UNIDENTIFIED_ERROR;
	}
	if (!WriteFile(serial_port, &address, 1, &io_result, 0) || io_result != 1)
	{
		error = GetLastError();
		return error ? error : ERROR_UNIDENTIFIED_ERROR;
	}
	for (DWORD size = (DWORD)message_size, written = 0; written != size;)
		if (WriteFile(serial_port, (const BYTE*)message + written, 1, &io_result, 0) && io_result)
			written += io_result;
		else
		{
			error = GetLastError();
			return error ? error : ERROR_UNIDENTIFIED_ERROR;
		}
	return 0;
}

WCHAR* ansi_to_utf16(HANDLE heap, CHAR* ansi_string)
{
	int length = MultiByteToWideChar(CP_ACP, 0, ansi_string, -1, 0, 0);
	if (!length)
		return 0;
	WCHAR* utf16_string = (WCHAR*)HeapAlloc(heap, 0, (SIZE_T)length * sizeof(WCHAR));
	if (!MultiByteToWideChar(CP_ACP, 0, ansi_string, -1, utf16_string, length))
	{
		HeapFree(heap, 0, utf16_string);
		return 0;
	}
	return utf16_string;
}

CHAR* utf16_to_ansi(HANDLE heap, WCHAR* utf16_string)
{
	int length = WideCharToMultiByte(CP_ACP, 0, utf16_string, -1, 0, 0, 0, 0);
	if (!length)
		return 0;
	CHAR* ansi_string = (CHAR*)HeapAlloc(heap, 0, (SIZE_T)length * sizeof(WCHAR));
	if (!WideCharToMultiByte(CP_ACP, 0, utf16_string, -1, ansi_string, length, 0, 0))
	{
		HeapFree(heap, 0, utf16_string);
		return 0;
	}
	return ansi_string;
}

void main()
{
	DWORD error;
	HANDLE console = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	HANDLE heap = GetProcessHeap();
	if (!heap)
	{
		error = GetLastError();
		print(console, L"Error GetProcessHeap failed\n");
		ExitProcess((UINT)error);
	}
	SIZE_T argument_count;
	WCHAR** argument;
	error = get_process_arguments(heap, &argument_count, &argument);
	if (error)
	{
		print(console, L"Error get_process_arguments failed\n");
		ExitProcess((UINT)error);
	}
	WCHAR* serial_port;
	search_argument(argument_count, (const WCHAR**)argument, L"-p", L"--serial_port", (const WCHAR**)&serial_port);
	BOOL info = search_argument(argument_count, (const WCHAR**)argument, L"-i", L"--info", 0);
	BOOL receive_message = search_argument(argument_count, (const WCHAR**)argument, L"-r", L"--receive", 0);
	WCHAR* send_address;
	search_argument(argument_count, (const WCHAR**)argument, L"-a", L"--address", (const WCHAR**)&send_address);
	WCHAR* send_message;
	search_argument(argument_count, (const WCHAR**)argument, L"-s", L"--send", (const WCHAR**)&send_message);
	if (serial_port && info)
	{
		HANDLE serila_port;
		error = open_serial_port(serial_port, 9600, 0, 0, &serila_port);
		if (error)
		{
			print(console, L"Error open_serial_port failed\n");
			ExitProcess((UINT)error);
		}
		BYTE base_station_address;
		BYTE address;
		error = get_connection_info(serila_port, &base_station_address, &address);
		if (error)
		{
			print(console, L"Error get_connection_info failed\n");
			ExitProcess((UINT)error);
		}
		WCHAR info_string[47];
		for (WCHAR* d = info_string, * s = (WCHAR*)L"Base station address ** and device address **\n", * e = s + 47; s != e; ++s, ++d)
			*d = *s;
		info_string[21] = L"0123456789ABCDEF"[base_station_address >> 4];
		info_string[22] = L"0123456789ABCDEF"[base_station_address & 0xF];
		info_string[43] = L"0123456789ABCDEF"[address >> 4];
		info_string[44] = L"0123456789ABCDEF"[address & 0xF];
		print(console, info_string);
		ExitProcess(0);
	}
	if (serial_port && send_message && send_address)
	{
		if (lstrlenW(send_address) != 2 ||
			!((send_address[0] >= L'0' && send_address[0] <= L'9') || (send_address[0] >= L'A' && send_address[0] <= L'F')) ||
			!((send_address[1] >= L'0' && send_address[1] <= L'9') || (send_address[1] >= L'A' && send_address[1] <= L'F')))
		{
			print(console, L"Error invalid address\n");
			ExitProcess((UINT)ERROR_BAD_ARGUMENTS);
		}
		BYTE address = ((BYTE)(L'A' > send_address[0] ? send_address[0] - L'0' : 0xA + send_address[0] - L'A') << 4) | (BYTE)(L'A' > send_address[1] ? send_address[1] - L'0' : 0xA + send_address[1] - L'A');
		CHAR* ansi_send_message = utf16_to_ansi(heap, send_message);
		if (!ansi_send_message)
		{
			error = GetLastError();
			print(console, L"Error utf16_to_ansi failed\n");
			ExitProcess((UINT)error);
		}
		HANDLE serila_port;
		error = open_serial_port(serial_port, 9600, 0, 0, &serila_port);
		if (error)
		{
			print(console, L"Error open_serial_port failed\n");
			ExitProcess((UINT)error);
		}
		error = send(serila_port, address, lstrlenA(ansi_send_message), ansi_send_message);
		if (error)
		{
			print(console, L"Error send failed\n");
			ExitProcess((UINT)error);
		}
		ExitProcess(0);
	}
	else if (serial_port && receive_message)
	{
		CHAR* ansi_message = (CHAR*)HeapAlloc(heap, 0, 0x1000001);
		if (!ansi_message)
		{
			error = GetLastError();
			print(console, L"Error HeapAlloc failed\n");
			ExitProcess((UINT)error);
		}
		HANDLE serila_port;
		error = open_serial_port(serial_port, 9600, 0, 0, &serila_port);
		if (error)
		{
			print(console, L"Error open_serial_port failed\n");
			ExitProcess((UINT)error);
		}
		BYTE sender_address;
		SIZE_T message_size;
		error = receive(serila_port, &sender_address, 0x1000000, &message_size, ansi_message);
		if (error)
		{
			print(console, L"Error receive failed\n");
			ExitProcess((UINT)error);
		}
		ansi_message[message_size] = 0;
		WCHAR* message = ansi_to_utf16(heap, ansi_message);
		if (!message)
		{
			error = GetLastError();
			print(console, L"Error ansi_to_utf16 failed\n");
			ExitProcess((UINT)error);
		}
		WCHAR from_string[11];
		for (WCHAR* d = from_string, *s = (WCHAR*)L"\" from **\n", *e = s + 11; s != e; ++s, ++d)
			*d = *s;
		from_string[7] = L"0123456789ABCDEF"[sender_address >> 4];
		from_string[8] = L"0123456789ABCDEF"[sender_address & 0xF];
		print(console, L"Received message \"");
		print(console, message);
		print(console, from_string);
		ExitProcess(0);
	}
	else
	{
		print(console, L"Error invalid arguments\n");
		ExitProcess((UINT)ERROR_BAD_ARGUMENTS);
	}
	ExitProcess((UINT)ERROR_UNIDENTIFIED_ERROR);
}