/*
	Mbed OS ASK transmitter version version 1.4.0 2018-09-25 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".

	Description
		Simple ask transmitter for Mbed OS.
		The transmitter can be used to communicate with RadioHead library.

	Version history
		version 1.5.0 2018-09-25
			Valid frequencies are now extended to 1000, 1250, 2000, 2500, 3125 and 4000 Hz.
		version 1.3.2 2018-08-01
			Wired debug mode added.
		version 1.3.1 2018-07-13
			Transmitter initialization behavior improved.
		version 1.3.0 2018-07-13
			is_valid_frequency member function added.
		version 1.2.0 2018-07-04
			tx_address member variable added.
		version 1.1.0 2018-06-14
			Status member function added.
			Some unnecessary comments removed.
		version 1.0.8 2018-06-13
			Valid frequencies are now limited to 1000, 1250, 2500 and 3125 Hz.
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
#define ASK_TRANSMITTER_VERSION_MINOR 4
#define ASK_TRANSMITTER_VERSION_PATCH 0

#define ASK_TRANSMITTER_IS_VERSION_ATLEAST(h, m, l) ((((unsigned long)(h) << 16) | ((unsigned long)(m) << 8) | (unsigned long)(l)) <= ((ASK_TRANSMITTER_VERSION_MAJOR << 16) | (ASK_TRANSMITTER_VERSION_MINOR << 8) | ASK_TRANSMITTER_VERSION_PATCH))

#include "mbed.h"
#include "ask_CRC16.h"
#include <stddef.h>
#include <stdint.h>

#ifndef ASK_TRANSMITTER_BUFFER_SIZE
#define ASK_TRANSMITTER_BUFFER_SIZE 64
#endif
#define ASK_TRANSMITTER_MAXIMUM_MESSAGE_SIZE 0xF8
#define ASK_TRANSMITTER_BROADCAST_ADDRESS 0xFF

typedef struct ask_transmitter_status_t
{
	int tx_frequency;
	PinName tx_pin;
	uint8_t tx_address;
	bool initialized;
	bool active;
	size_t packets_send;
	size_t bytes_send;
} ask_transmitter_status_t;

class ask_transmitter_t
{
	public :
		ask_transmitter_t();
		
		ask_transmitter_t(int tx_frequency, PinName tx_pin);
		ask_transmitter_t(int tx_frequency, PinName tx_pin, uint8_t new_tx_address);
		// These constructors call init with same parameters.
		
		~ask_transmitter_t();
		
		bool init(int tx_frequency, PinName tx_pin);
		/*
			Description
				Initializes the transmitter object with given parameters. If the transmitter is already initialized it is reinitialized with the new parameters.
				The tx address of the transmitter is set to ASK_TRANSMITTER_BROADCAST_ADDRESS.
				This function fails if an initialized transmitter already exists.
				Value of tx_address is set to the new tx address.
			Parameters
				tx_frequency
					The frequency of the transmitter. This value is required to be valid frequency or 0, or the function fails.
					Valid frequencies are 1000, 1250, 2000, 2500, 3125 and 4000.
					If this parameter is 0 and the transmitter is initialized it will shutdown.
					If this parameter is 0 and the transmitter is not initialized it will not be initialized.
					The transmitter is not initialized after it is shutdown.
				tx_pin
					Mbed OS pin name for tx pin.
			Return
				If the function succeeds, the return value is true and false on failure.
		*/
		
		bool init(int tx_frequency, PinName tx_pin, uint8_t new_tx_address);
		/*
			Description
				Initializes the transmitter object with given parameters. If the transmitter is already initialized it is reinitialized with the new parameters.
				This function fails if an initialized transmitter already exists.
				Value of tx_address is set to the new tx address.
			Parameters
				tx_frequency
					The frequency of the transmitter. This value is required to be valid frequency or 0, or the function fails.
					Valid frequencies are 1000, 1250, 2000, 2500, 3125 and 4000.
					If this parameter is 0 and the transmitter is initialized it will shutdown.
					If this parameter is 0 and the transmitter is not initialized it will not be initialized.
					The transmitter is not initialized after it is shutdown.

				tx_pin
					Mbed OS pin name for tx pin.
				new_tx_address
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

		void status(ask_transmitter_status_t* current_status);
		/*
			Description
				Function queries the current status of the transmitter.
			Parameters
				current_status
					Pointer to variable that receives current stutus of the transmitter.
			Return
				No return value.
		*/

		static bool is_valid_frequency(int frequency);
		/*
			Description
				Function test if given frequency is valid for transmitter.
				Valid frequencies are listed on documentation of init function.
				This function does not require initialized transmitter.
			Parameters
				frequency
					Value of frequency specifies the frequency that is tested.
			Return
				returns true if given frequency is valid for transmitter, else return value is false.
		*/

		volatile uint8_t tx_address;
		// Value of tx_address specifies address of the transmitter.

	private :
		static void _tx_interrupt_handler();
		static uint8_t _high_nibble(uint8_t byte);
		static uint8_t _low_nibble(uint8_t byte);
		static uint8_t _encode_symbol(uint8_t _4bit_data);
		
		void _write_byte_to_buffer(uint8_t data);
		bool _read_byte_from_buffer(uint8_t* data);

		bool _is_initialized;
		CRC16 _kermit;
		gpio_t _tx_pin;
		size_t _packets_send;
		size_t _bytes_send;
		uint8_t _tx_output_symbol;
		volatile uint8_t _tx_output_symbol_bit_index;
		volatile size_t _tx_buffer_read_index;
		volatile size_t _tx_buffer_write_index;
		volatile uint8_t _tx_buffer[ASK_TRANSMITTER_BUFFER_SIZE];
		Ticker _tx_timer;

		// transmitter initialization parameters
		int _tx_frequency;
		PinName _tx_pin_name;

		// No copying object of this type!
		ask_transmitter_t(const ask_transmitter_t&);
		ask_transmitter_t& operator=(const ask_transmitter_t&);
};

#endif