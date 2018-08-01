/*
	Mbed OS ASK receiver version 1.4.1 2018-08-01 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".

	Description
		Simple ask receiver for Mbed OS.
		The receiver can be used to communicate with RadioHead library.

	Version history
		version 1.4.1 2018-08-01
			rx_entropy bit mixing improved.
		version 1.4.0 2018-07-19
			Added new overload for init and constructor.
		version 1.3.2 2018-07-13
			recv member function behavior improved.
		version 1.3.1 2018-07-13
			Receiver initialization behavior improved.
		version 1.3.0 2018-07-13
			rx_entropy member variable and is_valid_frequency member function added.
		version 1.2.0 2018-07-04
			rx_address member variable added.
		version 1.1.1 2018-06-18
			Receiver ignores packets that are not send to it.
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

#define ASK_RECEIVER_VERSION_MAJOR 1
#define ASK_RECEIVER_VERSION_MINOR 4
#define ASK_RECEIVER_VERSION_PATCH 1

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
	bool receive_all_packets;
	bool active;
	int packets_available;
	size_t packets_received;
	size_t packets_dropped;
	size_t bytes_received;
	size_t bytes_dropped;
	uint32_t rx_entropy;
} ask_receiver_status_t;

class ask_receiver_t
{
	public :
		ask_receiver_t();
		ask_receiver_t(int rx_frequency, PinName rx_pin);
		ask_receiver_t(int rx_frequency, PinName rx_pin, uint8_t new_rx_address);
		ask_receiver_t(int rx_frequency, PinName rx_pin, uint8_t new_rx_address, bool receive_all_packets);
		// These constructors call init with same parameters.

		~ask_receiver_t();

		bool init(int rx_frequency, PinName rx_pin);
		/*
			Description
				Re/initializes the receiver object with given parameters.
				Re/initializing receiver object will fail if initialized receiver object already exists.
				The rx address of the receiver is set to ASK_RECEIVER_BROADCAST_ADDRESS.
				Value of rx_address is set to the new rx address.
			Parameters
				rx_frequency
					The frequency of the receiver. This value is required to be valid frequency or 0, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
					If this parameter is 0 and the receiver is initialized it will shutdown.
					If this parameter is 0 and the receiver is not initialized it will not be initialized.
					The receiver is not initialized after it is shutdown.
				rx_pin
					Mbed OS pin name for rx pin.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/

		bool init(int rx_frequency, PinName rx_pin, uint8_t new_rx_address);
		/*
			Descriptions
				Re/initializes the receiver object with given parameters.
				Re/initializing receiver object will fail if initialized receiver object already exists.
				Value of rx_address is set to the new rx address.
			Parameters
				rx_frequency
					The frequency of the receiver. This value is required to be valid frequency or 0, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
					If this parameter is 0 and the receiver is initialized it will shutdown.
					If this parameter is 0 and the receiver is not initialized it will not initialize.
					The receiver is not initialized after it is shutdown.
				rx_pin
					Mbed OS pin name for rx pin.
				new_rx_address
					rx address for the receiver.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/

		bool init(int rx_frequency, PinName rx_pin, uint8_t new_rx_address, bool receive_all_packets);
		/*
			Descriptions
				Re/initializes the receiver object with given parameters.
				Re/initializing receiver object will fail if initialized receiver object already exists.
				Value of rx_address is set to the new rx address.
			Parameters
				rx_frequency
					The frequency of the receiver. This value is required to be valid frequency or 0, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
					If this parameter is 0 and the receiver is initialized it will shutdown.
					If this parameter is 0 and the receiver is not initialized it will not initialize.
					The receiver is not initialized after it is shutdown.
				rx_pin
					Mbed OS pin name for rx pin.
				new_rx_address
					rx address for the receiver.
				receive_all_packets
					If value of receive_all_packets is false receiver receives only packets that are send to broadcast address or receiver's rx address.
					If value of receive_all_packets is true receiver receives all packets.
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

		size_t recv(uint8_t* rx_address, uint8_t* tx_address, void* message_buffer, size_t message_buffer_length);
		/*
			Description
				Function Reads packet from receiver's buffer if there are any available packets, if not function returns 0.
				Receiver's interrupt handler writes packets that it receives to receiver's buffer.
				The receiver does not receive any packets if it is not initialized.
				If the packet is longer than the size of caller's buffer, this function truncates the packet by message_buffer_length parameter.
			Parameters
				tx_address
					Pointer to variable that receives address of the receiver.
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

		static bool is_valid_frequency(int frequency);
		/*
			Description
				Function test if given frequency is valid for receiver.
				Valid frequencies are listed on documentation of init function.
				This function does not require initialized receiver.
			Parameters
				frequency
					Value of frequency specifies the frequency that is tested.
			Return
				returns true if given frequency is valid for receiver, else return value is false.
		*/

		volatile uint8_t rx_address;
		// Value of rx_address specifies address of the receiver.

		volatile uint32_t rx_entropy;
		// Value of rx_entropy is mix of all samples that the receiver reads from rx pin.
		// This variable is updated rx_frequency * ASK_RECEIVER_SAMPLERS_PER_BIT times every second by the Receiver's interrupt handler, while the receiver initialized.

	private :
		static void _rx_interrupt_handler();
		static uint8_t _decode_symbol(uint8_t _6bit_symbol);
		size_t _get_buffer_free_space();
		void _write_byte_to_buffer(uint8_t data);
		void _erase_current_packet();
		uint8_t _read_byte_from_buffer();
		void _discard_bytes_from_buffer(size_t size);

		bool _is_initialized;
		CRC16 _kermit;
		gpio_t _rx_pin;
		Ticker _rx_timer;

		volatile int _packets_available;
		uint8_t _rx_last_sample;
		uint8_t _rx_ramp;
		uint8_t _rx_integrator;
		unsigned int _rx_bits;
		bool _receive_all_packets;
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