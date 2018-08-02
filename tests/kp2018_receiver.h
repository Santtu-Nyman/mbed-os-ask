#include "mbed.h"
#include "ask_tdma.h"

void kp2018_receiver()
{
	const int f = 1000;
	Serial pc(USBTX, USBRX);
	uint8_t message_buffer[0x400];
	ask_tdma_client_t client;
	client.init(D4, D2, f);
	uint8_t base_station_address;
	client.get_base_station_address(&base_station_address);
	uint8_t address;
	client.get_address(&address);
	for (;;)
	{
		uint8_t rx_address;
		uint8_t tx_address;
		size_t message_length;
		client.recv(0, 0x3FF, message_buffer, &rx_address, &tx_address, &message_length);
		message_buffer[message_length] = 0;
		pc.printf("%02X->%02X \"%s\"\n", tx_address, rx_address, message_buffer);
	}
}