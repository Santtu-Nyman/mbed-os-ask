/*
	Mbed OS ASK transmitter version 1.0.8 2018-06-13 by Santtu Nyman.

	Description
		Simple ask transmitter for Mbed OS.
		The transmitter can be used to communicate with RadioHead library.

	Version history
		version 1.0.8 2018-06-13
			Valid frequencies are now limited to 1000, 1250, 2500 and 3125.
		version 1.0.7 2018-06-11
			frequencies in list of valid frequencies are now valid.
		version 1.0.6 2018-06-11
	  		Existing CRC function moved to CRC16.h header
	  		CRC16.h header integrated.
		version 1.0.5 2018-06-05
			CRC function documented.
			Transmitter public member function documentation inproved.
		version 1.0.4 2018-06-04
			Transmitter public member function use now documented.
			Nibble extraction functions added.
		version 1.0.3 2018-06-01
			Initialization is now more readable and packet length computation commented.
		version 1.0.2 2018-05-28
			Macros prefixed by ASK_ are now prefixed by ASK_TRANSMITTER_.
		version 1.0.1 2018-05-28
			Unused code removed and transmitter frequency parameter validation added.
		version 1.0.0 2018-05-23
			Everything rewritten with simpler implementation and RadioHead like interface.
		version 0.2.0 2018-05-18
			Added support for addresses other than broadcast addresses.
		version 0.1.0 2018-05-17
			Testing some interface ideas and stuff.
*/

#ifndef ASK_TRANSMITTER_H
#define ASK_TRANSMITTER_H

#define ASK_TRANSMITTER_VERSION_MAJOR 1
#define ASK_TRANSMITTER_VERSION_MINOR 0
#define ASK_TRANSMITTER_VERSION_PATCH 8

#define ASK_TRANSMITTER_IS_VERSION_ATLEAST(h, m, l) ((((unsigned long)(h) << 16) | ((unsigned long)(m) << 8) | (unsigned long)(l)) <= ((ASK_TRANSMITTER_VERSION_MAJOR << 16) | (ASK_TRANSMITTER_VERSION_MINOR << 8) | ASK_TRANSMITTER_VERSION_PATCH))

#include "mbed.h"
#include "CRC16.h" // JARNO
#include <stddef.h>
#include <stdint.h>

#ifndef ASK_TRANSMITTER_BUFFER_SIZE
#define ASK_TRANSMITTER_BUFFER_SIZE 64
#endif
#define ASK_TRANSMITTER_MAXIMUM_MESSAGE_SIZE 0xF8
#define ASK_TRANSMITTER_BROADCAST_ADDRESS 0xFF

class ask_transmitter_t
{
	public :
		ask_transmitter_t();
		
		ask_transmitter_t(int tx_frequency, PinName tx_pin);
		ask_transmitter_t(int tx_frequency, PinName tx_pin, uint8_t tx_address);
		// These constructors call init with same parameters.
		
		~ask_transmitter_t();
		
		bool init(int tx_frequency, PinName tx_pin);
		/*
			Description
				Initializes the transmitter object with given parameters. If the transmitter is already initialized it is reinitialized with the new parameters.
				The tx address of the transmitter is set to ASK_TRANSMITTER_BROADCAST_ADDRESS.
				This function fails if an initialized transmitter already exists.
			Parameters
				tx_frequency
					The frequency of the transmitter. This value is required to be valid frequency, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
				tx_pin
					Mbed OS pin name for tx pin.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/
		
		bool init(int tx_frequency, PinName tx_pin, uint8_t tx_address);
		/*
			Description
				Initializes the transmitter object with given parameters. If the transmitter is already initialized it is reinitialized with the new parameters.
				This function fails if an initialized transmitter already exists.
			Parameters
				tx_frequency
					The frequency of the transmitter. This value is required to be valid frequency, or the function fails.
					Valid frequencies are 1000, 1250, 2500 and 3125.
				tx_pin
					Mbed OS pin name for tx pin.
				tx_address
					tx address for the transmitter.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/
		
		bool send(uint8_t rx_address, const void* message_data, size_t message_byte_length);
		/*
			Description
				Writes packet with given message to the buffer of the transmitter, which is then sent by the interrupt handler.
				This function will block, if not enough space for the packet in buffer.
				The transmitter is required to be initialized or this function will fail.
			Parameters
				rx_address
					Address of the receiver.
				message_data
					Pointer to the data to by send.
				message_byte_length
					The number of bytes to be send.
					maximum value for this parameter is ASK_TRANSMITTER_MAXIMUM_MESSAGE_SIZE.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/
		
		bool send(const void* message_data, size_t message_byte_length);
		/*
			Description
				Writes packet with given message to the buffer of the transmitter, which is then sent by the interrupt handler.
				This function will block, if not enough space for the packet in buffer.
				Packet is send to the broadcast address.
				The transmitter is required to be initialized or this function will fail.
			Parameters
				message_data
					Pointer to the data to by send.
				message_byte_length
					The number of bytes to be send.
					maximum value for this parameter is ASK_TRANSMITTER_MAXIMUM_MESSAGE_SIZE.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/
	private :
		static void _tx_interrupt_handler();
		static uint8_t _high_nibble(uint8_t byte);
		static uint8_t _low_nibble(uint8_t byte);
		static uint8_t _encode_symbol(uint8_t _4bit_data);
		//static uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data); // JARNO
		
		void _write_byte_to_buffer(uint8_t data);
		bool _read_byte_from_buffer(uint8_t* data);

		bool _is_initialized;
		CRC16 _kermit; // JARNO
		uint8_t _tx_address;
		gpio_t _tx_pin;
		Ticker _tx_timer;
		uint8_t _tx_output_symbol_bit_index;
		uint8_t _tx_output_symbol;
		volatile size_t _tx_buffer_read_index;
		volatile size_t _tx_buffer_write_index;
		volatile uint8_t _tx_buffer[ASK_TRANSMITTER_BUFFER_SIZE];

		// transmitter initialization parameters
		int _tx_frequency;
		PinName _tx_pin_name;

		// No copying object of this type!
		ask_transmitter_t(const ask_transmitter_t&);
		ask_transmitter_t& operator=(const ask_transmitter_t&);
};

#endif