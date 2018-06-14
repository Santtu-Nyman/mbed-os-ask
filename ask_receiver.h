/*
	Mbed OS ASK receiver version 1.1.0 2018-06-14 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".

	Description
		Simple ask receiver for Mbed OS.
		The receiver can be used to communicate with RadioHead library.

	Version history
		version 1.1.0 2018-06-14
			Status member function added.
			Forgotten debug feature removed.
			Some unnecessary comments removed.
		version 1.0.3 2018-06-13
			Valid frequencies are now limited to 1000, 1250, 2500 and 3125.
		version 1.0.2 2018-06-11
			frequencies in list of valid frequencies are now valid.
		version 1.0.1 2018-06-11	  	
			Existing CRC function moved to CRC16.h header
			CRC16.h header integrated.
		version 1.0.0 2018-06-05
			first

*/

#ifndef ASK_RECEIVER_H
#define ASK_RECEIVER_H

#define ASK_TRANSMITTER_VERSION_MAJOR 1
#define ASK_TRANSMITTER_VERSION_MINOR 1
#define ASK_TRANSMITTER_VERSION_PATCH 0

#define ASK_RECEIVER_IS_VERSION_ATLEAST(h, m, l) ((((unsigned long)(h) << 16) | ((unsigned long)(m) << 8) | (unsigned long)(l)) <= ((ASK_RECEIVER_VERSION_MAJOR << 16) | (ASK_RECEIVER_VERSION_MINOR << 8) | ASK_RECEIVER_VERSION_PATCH))

#include "mbed.h"
#include "ask_CRC16.h"
#include <stddef.h>
#include <stdint.h>

#ifndef ASK_RECEIVER_BUFFER_SIZE
#define ASK_RECEIVER_BUFFER_SIZE 64
#endif
#define ASK_RECEIVER_MAXIMUM_MESSAGE_SIZE 0xF8
#define ASK_RECEIVER_BROADCAST_ADDRESS 0xFF
#define ASK_RECEIVER_SAMPLERS_PER_BIT 8

#define ASK_RECEIVER_START_SYMBOL 0xB38

#define ASK_RECEIVER_RAMP_LENGTH 160
#define ASK_RECEIVER_RAMP_INCREMENT (ASK_RECEIVER_RAMP_LENGTH / ASK_RECEIVER_SAMPLERS_PER_BIT)
#define ASK_RECEIVER_RAMP_TRANSITION (ASK_RECEIVER_RAMP_LENGTH / 2)
#define ASK_RECEIVER_RAMP_ADJUST ((ASK_RECEIVER_RAMP_INCREMENT / 2) - 1)
#define ASK_RECEIVER_RAMP_INCREMENT_RETARD (ASK_RECEIVER_RAMP_INCREMENT - ASK_RECEIVER_RAMP_ADJUST)
#define ASK_RECEIVER_RAMP_INCREMENT_ADVANCE (ASK_RECEIVER_RAMP_INCREMENT + ASK_RECEIVER_RAMP_ADJUST)

typedef struct ask_receiver_status_t
{
	int rx_frequency;
	PinName rx_pin;
	uint8_t rx_address;
	bool initialized;
	bool active;
	int packets_available;
	size_t packets_received;
	size_t packets_dropped;
	size_t bytes_received;
	size_t bytes_dropped;
} ask_receiver_status_t;

class ask_receiver_t
{
	public :
		ask_receiver_t();
		ask_receiver_t(int rx_frequency, PinName rx_pin);
		ask_receiver_t(int rx_frequency, PinName rx_pin, uint8_t rx_address);
		// These constructors call init with same parameters.

		~ask_receiver_t();

		bool init(int rx_frequency, PinName rx_pin);
		/*
			Description
				Re/initializes the receiver object with given parameters.
				Re/initializing receiver object will fail if initialized receiver object already exists.
				The rx address of the receiver is set to ASK_RECEIVER_BROADCAST_ADDRESS.
			Parameters
				rx_frequency
					The frequency of the receiver. This value is required to be valid frequency, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
				rx_pin
					Mbed OS pin name for rx pin.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/

		bool init(int rx_frequency, PinName rx_pin, uint8_t rx_address);
		/*
			Description
				Re/initializes the receiver object with given parameters.
				Re/initializing receiver object will fail if initialized receiver object already exists.
			Parameters
				rx_frequency
					The frequency of the receiver. This value is required to be valid frequency, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
				rx_pin
					Mbed OS pin name for rx pin.
				rx_address
					rx address for the receiver.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/

		size_t recv(void* message_buffer, size_t message_buffer_length);
		/*
			Description
				Function Reads packet from receiver's buffer if there are any available packets, if not function returns 0.
				Receiver's interrupt handler writes packets that it receives to receiver's buffer.
				The receiver does not receive any packets if it is not initialized.
				If the packet is longer than the size of caller's buffer, this function truncates the packet by message_buffer_length parameter.
			Parameters
				message_buffer
					Pointer to buffer that receives packest data.
				message_buffer_length
					Size of buffer pointed by message_data.
					maximum size if packet is ASK_RECEIVER_MAXIMUM_MESSAGE_SIZE.
					passing 0 value to this parameter makes it impossible to determine if packet was read, this may be undesired behavior.
			Return
				If function reads a packet it returns size of the packet truncated to size of callers buffer.
				If no packet is read it returns 0.
		*/

		size_t recv(uint8_t* tx_address, void* message_buffer, size_t message_buffer_length);
		/*
			Description
				Function Reads packet from receiver's buffer if there are any available packets, if not function returns 0.
				Receiver's interrupt handler writes packets that it receives to receiver's buffer.
				The receiver does not receive any packets if it is not initialized.
				If the packet is longer than the size of caller's buffer, this function truncates the packet by message_buffer_length parameter.
			Parameters
				tx_address
					Pointer to variable that receives address of the transmitter.
				message_buffer
					Pointer to buffer that receives packest data.
				message_buffer_length
					Size of buffer pointed by message_data.
					maximum size of packet is ASK_RECEIVER_MAXIMUM_MESSAGE_SIZE.
					passing 0 value to this parameter makes it impossible to determine if packet was read, this may be undesired behavior.
			Return
				If function reads a packet it returns size of the packet truncated to size of callers buffer.
				If no packet is read it returns 0.
		*/

		void status(ask_receiver_status_t* current_status);
		/*
			Description
				Function queries the current status of the receiver.
			Parameters
				current_status
					Pointer to variable that receives current stutus of the receiver.
			Return
				No return value.
		*/

	private :
		static void _rx_interrupt_handler();
		static uint8_t _decode_symbol(uint8_t _6bit_symbol);
		size_t _get_buffer_free_space();
		void _write_byte_to_buffer(uint8_t data);
		void _erase_current_packet();
		uint8_t _read_byte_from_buffer();
		void _discard_bytes_from_buffer(size_t size);

		bool _is_initialized;
		uint8_t _rx_address;
		CRC16 _kermit;
		gpio_t _rx_pin;
		Ticker _rx_timer;

		volatile int _packets_available;
		uint8_t _rx_last_sample;
		uint8_t _rx_ramp;
		uint8_t _rx_integrator;
		unsigned int _rx_bits;
		volatile uint8_t _rx_active;
		uint8_t _rx_bit_count;
		uint8_t _packet_length;
		uint8_t _packet_received;
		uint16_t _packet_crc;
		uint16_t _packet_received_crc;
		volatile size_t _packets_received;
		volatile size_t _packets_dropped;
		volatile size_t _bytes_received;
		volatile size_t _bytes_dropped;

		// input ring buffer
		volatile size_t _rx_buffer_read_index;
		volatile size_t _rx_buffer_write_index;
		volatile uint8_t _rx_buffer[ASK_RECEIVER_BUFFER_SIZE];

		// receiver initialization parameters
		int _rx_frequency;
		PinName _rx_pin_name;

		// No copying object of this type!
		ask_receiver_t(const ask_receiver_t&);
		ask_receiver_t& operator=(const ask_receiver_t&);
};

#endif