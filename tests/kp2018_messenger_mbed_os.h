#include "mbed.h"
#include "ask_tdma.h"

void kp2018_messenger_stop()
{
	for (;;)
		wait_us(1000000);
}

void kp2018_messenger()
{
	const PinName error_notification_pin = LED1;
	const PinName rx = D4;
	const PinName tx = D2;
	const int f = 1000;

	uint8_t message_buffer[0x400];

	DigitalOut error_notification(error_notification_pin, 0);

	Serial pc(USBTX, USBRX);

	ask_tdma_client_t client;
	int error = client.init(rx, tx, f);
	if (error)
	{
		error_notification = 1;
		kp2018_messenger_stop();
	}

	uint8_t base_station_address;
	error = client.get_base_station_address(&base_station_address);
	if (error)
	{
		error_notification = 1;
		kp2018_messenger_stop();
	}

	uint8_t address;
	error = client.get_address(&address);
	if (error)
	{
		error_notification = 1;
		kp2018_messenger_stop();
	}

	for (;;)
	{
		if (pc.readable())
		{
			uint8_t c = pc.getc();
			if (c == 0xFF)
			{
				client.get_base_station_address(&base_station_address);
				client.get_address(&address);
				pc.putc(base_station_address);
				pc.putc(address);
			}
			else if (c == 0xFE)
			{
				uint8_t rx_address;
				uint8_t tx_address;
				size_t message_length;
				if (!client.recv(0, 0x400, message_buffer, &rx_address, &tx_address, &message_length))
				{
					if (message_length > 0xFF)
						message_length = 0xFF;
					pc.putc((uint8_t)message_length);
					pc.putc(tx_address);
					for (uint8_t message_uploaded = 0; message_uploaded != (uint8_t)message_length; ++message_uploaded)
						pc.putc(message_buffer[message_uploaded]);
				}
				else
				{
					error_notification = 1;
					wait_us(1000000);
					error_notification = 0;
					pc.putc(0);
					pc.putc(ASK_RECEIVER_BROADCAST_ADDRESS);
				}
			}
			else if (c == 0xFD)
			{
				while (!pc.readable())
					continue;
				uint8_t message_length = pc.getc();
				while (!pc.readable())
					continue;
				uint8_t message_receiver = pc.getc();
				for (uint8_t message_loaded = 0; message_loaded != message_length; ++message_loaded)
				{
					while (!pc.readable())
						continue;
					message_buffer[message_loaded] = pc.getc();
				}
				size_t message_send;
				if (client.send(message_receiver, message_length, message_buffer, &message_send))
				{
					error_notification = 1;
					wait_us(1000000);
					error_notification = 0;
				}
			}
		}
	}
}