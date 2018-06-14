/*
	Mbed OS ASK transmitter version 1.1.0 2018-06-14 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".
*/

#include "ask_transmitter.h"

// pointer to the transmitter for interrupt handler
static ask_transmitter_t* _ask_transmitter;

ask_transmitter_t::ask_transmitter_t()
{
	_is_initialized = false;
}

ask_transmitter_t::ask_transmitter_t(int tx_frequency, PinName tx_pin)
{
	_is_initialized = false;
	init(tx_frequency, tx_pin);
}

ask_transmitter_t::ask_transmitter_t(int tx_frequency, PinName tx_pin, uint8_t tx_address)
{
	_is_initialized = false;
	init(tx_frequency, tx_pin, tx_address);
}

ask_transmitter_t::~ask_transmitter_t()
{
	if (_is_initialized)
	{
		_tx_timer.detach();
		_ask_transmitter = 0;
	}
}

bool ask_transmitter_t::init(int tx_frequency, PinName tx_pin)
{
	return init(tx_frequency, tx_pin, ASK_TRANSMITTER_BROADCAST_ADDRESS);
}

bool ask_transmitter_t::init(int tx_frequency, PinName tx_pin, uint8_t tx_address)
{
	static const int valid_frequencies[] = { 1000, 1250, 2500, 3125 };

	// search valid frequency list for the value of frequency parameter
	bool invalid_frequency = true;
	for (int i = 0, e = sizeof(valid_frequencies) / sizeof(int); invalid_frequency && i != e; ++i)
		if (tx_frequency == valid_frequencies[i])
			invalid_frequency = false;

	// fail init if invalid frequency
	if (invalid_frequency)
		return false;

	// only one transmitter is allowed after version 0.2.0 for simpler implementation this one transmitter is pointed by _ask_transmitter
	if (!_ask_transmitter)
		_ask_transmitter = this;

	// this must be THE transmitter
	if (this == _ask_transmitter)
	{
		// if reinitializing detach the interrupt handler
		if (_is_initialized)
			_tx_timer.detach();

		_kermit = CRC16("KERMIT", 0x1021, 0x0000, 0x0000, true, true, FAST_CRC);
		_tx_address = tx_address;

		// set transmitter initialization parameters
		_tx_frequency = tx_frequency;
		_tx_pin_name = tx_pin;

		_packets_send = 0;
		_bytes_send = 0;

		// set ring buffer indices to 0
		_tx_output_symbol_bit_index = 0;
		_tx_buffer_read_index = 0;
		_tx_buffer_write_index = 0;

		_is_initialized = true;

		// init tx output pin
		gpio_init_out_ex(&_tx_pin, _tx_pin_name, 0);
		
		// attach the interrupt handler
		_tx_timer.attach(&_tx_interrupt_handler, 1.0f / (float)tx_frequency);
	}
	return _is_initialized;
}

bool ask_transmitter_t::send(uint8_t rx_address, const void* message_data, size_t message_byte_length)
{
	static const uint8_t preamble_and_start_symbol[8] = { 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x38, 0x2C };

	if (message_byte_length > ASK_TRANSMITTER_MAXIMUM_MESSAGE_SIZE || !_is_initialized)
		return false;

	// the data after the start symbol begins with length of the packet, header rx address, header tx address, header id, header flags
	// lenght of the packet is (1 byte lenght + 1 byte rx address + 1 byte tx ddress + 1 byte id + 1 byte flags + n bytes message + 2 bytes crc)
	uint8_t length_and_header[5] = { (uint8_t)(7 + message_byte_length), rx_address, _tx_address, 0, 0, };

	// crc init is 0xFFFF
	uint16_t crc = 0xFFFF;

	// write preamble and start symbol to output buffer
	for (size_t i = 0; i != sizeof(preamble_and_start_symbol); ++i)
		_write_byte_to_buffer(preamble_and_start_symbol[i]);

	uint8_t next_byte;

	// write length and header to output buffer
	for (size_t i = 0; i != sizeof(length_and_header); ++i)
	{
		next_byte = length_and_header[i];
		crc = _kermit.fastCRC(crc, next_byte);
		_write_byte_to_buffer(_encode_symbol(_high_nibble(next_byte)));
		_write_byte_to_buffer(_encode_symbol(_low_nibble(next_byte)));
	}

	// write message data to output buffer
	for (size_t i = 0; i != message_byte_length; ++i)
	{
		next_byte = *(const uint8_t*)((uintptr_t)message_data + i);
		crc = _kermit.fastCRC(crc, next_byte);
		_write_byte_to_buffer(_encode_symbol(_high_nibble(next_byte)));
		_write_byte_to_buffer(_encode_symbol(_low_nibble(next_byte)));
	}

	// crc xorout is 0xFFFF
	crc ^= 0xFFFF;

	// write crc to output buffer in little endian byte order
	next_byte = (uint8_t)(crc & 0xFF);
	_write_byte_to_buffer(_encode_symbol(_high_nibble(next_byte)));
	_write_byte_to_buffer(_encode_symbol(_low_nibble(next_byte)));
	next_byte = (uint8_t)(crc >> 8);
	_write_byte_to_buffer(_encode_symbol(_high_nibble(next_byte)));
	_write_byte_to_buffer(_encode_symbol(_low_nibble(next_byte)));

	// write 0 after the packet to set output low after the packet is send
	_write_byte_to_buffer(0);

	++_packets_send;
	_bytes_send += message_byte_length;

	return true;
}

bool ask_transmitter_t::send(const void* message_data, size_t message_byte_length)
{
	return send(ASK_TRANSMITTER_BROADCAST_ADDRESS, message_data, message_byte_length);
}

void ask_transmitter_t::status(ask_transmitter_status_t* current_status)
{
	if (_is_initialized)
	{
		current_status->tx_frequency = _tx_frequency;
		current_status->tx_pin = _tx_pin_name;
		current_status->tx_address = _tx_address;
		current_status->initialized = true;
		if (_tx_buffer_read_index != _tx_buffer_write_index || _tx_output_symbol_bit_index)
			current_status->active = true;
		else
			current_status->active = false;
		current_status->packets_send = _packets_send;
		current_status->bytes_send = _bytes_send;
	}
	else
	{
		current_status->tx_frequency = 0;
		current_status->tx_pin = NC;
		current_status->tx_address = ASK_TRANSMITTER_BROADCAST_ADDRESS;
		current_status->initialized = false;
		current_status->active = false;
		current_status->packets_send = 0;
		current_status->bytes_send = 0;
	}
}

void ask_transmitter_t::_tx_interrupt_handler()
{
	// read next byte if !symbol_bit_index and if no data to send return from this function
	uint8_t symbol_bit_index = _ask_transmitter->_tx_output_symbol_bit_index;
	if (!symbol_bit_index && !_ask_transmitter->_read_byte_from_buffer(&_ask_transmitter->_tx_output_symbol))
		return;

	// send next bit if there is more data to send.
	
	uint8_t symbol = _ask_transmitter->_tx_output_symbol;
	
	// set the tx pin voltage(low or high) to the value of bit number symbol_bit_index of symbol and incroment symbol_bit_index to index of next bit.
	gpio_write(&_ask_transmitter->_tx_pin, (int)((symbol >> symbol_bit_index++) & 1));

	// after sending 6 low bits of current byte start sending next byte
	if (symbol_bit_index == 6)
		symbol_bit_index = 0;
	_ask_transmitter->_tx_output_symbol_bit_index = symbol_bit_index;
}

uint8_t ask_transmitter_t::_high_nibble(uint8_t byte)
{
	return byte >> 4;
}

uint8_t ask_transmitter_t::_low_nibble(uint8_t byte)
{
	return byte & 0xF;
}

uint8_t ask_transmitter_t::_encode_symbol(uint8_t _4bit_data)
{
	static const uint8_t symbol_table[16] = { 0x0D, 0x0E, 0x13, 0x15, 0x16, 0x19, 0x1A, 0x1C, 0x23, 0x25, 0x26, 0x29, 0x2A, 0x2C, 0x32, 0x34 };
	return symbol_table[_4bit_data];
}

void ask_transmitter_t::_write_byte_to_buffer(uint8_t data)
{
	// wait for empty space in the buffer and then write a byte to it
	for (;;)
	{
		size_t maximum_write_index = _tx_buffer_read_index;
		if (maximum_write_index)
			--maximum_write_index;
		else
			maximum_write_index = ASK_TRANSMITTER_BUFFER_SIZE - 1;
		size_t write_index = _tx_buffer_write_index;
		if (write_index != maximum_write_index)
		{
			_tx_buffer[write_index++] = data;
			if (write_index != ASK_TRANSMITTER_BUFFER_SIZE)
				_tx_buffer_write_index = write_index;
			else
				_tx_buffer_write_index = 0;
			return;
		}
	}
}

bool ask_transmitter_t::_read_byte_from_buffer(uint8_t* data)
{
	// read next available byte from the buffer if there is any available bytes
	size_t read_index = _tx_buffer_read_index;
	if (read_index != _tx_buffer_write_index)
	{
		*data = _tx_buffer[read_index++];
		if (read_index != ASK_TRANSMITTER_BUFFER_SIZE)
			_tx_buffer_read_index = read_index;
		else
			_tx_buffer_read_index = 0;
		return true;
	}
	return false;
}