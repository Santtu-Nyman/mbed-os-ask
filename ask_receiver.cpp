/*
	Mbed OS ASK receiver version 1.4.1 2018-08-01 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".
*/

#include "ask_receiver.h"

// pointer to the receiver for interrupt handler
static ask_receiver_t* _ask_receiver;

ask_receiver_t::ask_receiver_t()
{
	_is_initialized = false;
}

ask_receiver_t::ask_receiver_t(int rx_frequency, PinName rx_pin)
{
	_is_initialized = false;
	init(rx_frequency, rx_pin);
}

ask_receiver_t::ask_receiver_t(int rx_frequency, PinName rx_pin, uint8_t new_rx_address)
{
	_is_initialized = false;
	init(rx_frequency, rx_pin, new_rx_address);
}

ask_receiver_t::ask_receiver_t(int rx_frequency, PinName rx_pin, uint8_t new_rx_address, bool receive_all_packets)
{
	_is_initialized = false;
	init(rx_frequency, rx_pin, new_rx_address, receive_all_packets);
}

ask_receiver_t::~ask_receiver_t()
{
	init(0, NC, ASK_RECEIVER_BROADCAST_ADDRESS, false);
}

bool ask_receiver_t::init(int rx_frequency, PinName rx_pin)
{
	return init(rx_frequency, rx_pin, ASK_RECEIVER_BROADCAST_ADDRESS, false);
}

bool ask_receiver_t::init(int rx_frequency, PinName rx_pin, uint8_t new_rx_address)
{
	return init(rx_frequency, rx_pin, new_rx_address, false);
}

bool ask_receiver_t::init(int rx_frequency, PinName rx_pin, uint8_t new_rx_address, bool receive_all_packets)
{
	// shutdown if rx_frequency is 0
	if (!rx_frequency)
	{
		// if receiver is initialized detach the interrupt handler and disconnect rx pin
		if (_is_initialized)
		{
			_rx_timer.detach();
			gpio_init_in(&_rx_pin, NC);
			_is_initialized = false;
		}
		return true;
	}

	// fail rx pin is not connected
	if (rx_pin == NC)
		return false;

	// fail init if invalid frequency
	if (!is_valid_frequency(rx_frequency))
		return false;

	// only one receiver is allowed this one receiver is pointed by _ask_receiver
	if (!_ask_receiver)
		_ask_receiver = this;

	// this must be THE receiver
	if (this == _ask_receiver)
	{
		// if reinitializing detach the interrupt handler and disconnect rx pin
		if (_is_initialized)
		{
			_rx_timer.detach();
			gpio_init_in(&_rx_pin, NC);
		}

		_kermit = CRC16(0x1021, 0x0000, 0x0000, true, true, FAST_CRC);
		rx_address = new_rx_address;

		// set receiver initialization parameters
		_rx_frequency = rx_frequency;
		_rx_pin_name = rx_pin;

		_packets_available = 0;
		_rx_last_sample = 0;
		_rx_ramp = 0;
		_rx_integrator = 0;
		_rx_bits = 0;
		_receive_all_packets = receive_all_packets;
		_rx_active = 0;

		// if reinitializing do not reinitialize rx entropy
		if (!_is_initialized)
		{
			// init rx entropy source is fliped crc32 initial value
			rx_entropy = 0;
		}

		_packets_received = 0;
		_packets_dropped = 0;
		_bytes_received = 0;
		_bytes_dropped = 0;

		// set ring buffer indices to 0
		_rx_buffer_read_index = 0;
		_rx_buffer_write_index = 0;

		_is_initialized = true;

		// init rx input pin
		gpio_init_in(&_rx_pin, _rx_pin_name);

		// attach the interrupt handler
		// receiver interrupt frequency needs to be multipled by samples per bit

		_rx_timer.attach(&_rx_interrupt_handler, 1.0f / (float)(rx_frequency * ASK_RECEIVER_SAMPLERS_PER_BIT));
	}
	return _is_initialized;
}

size_t ask_receiver_t::recv(void* message_buffer, size_t message_buffer_length)
{
	uint8_t ingnored[2];
	return recv(&ingnored[0], &ingnored[1], message_buffer, message_buffer_length);
}

size_t ask_receiver_t::recv(uint8_t* tx_address, void* message_buffer, size_t message_buffer_length)
{
	uint8_t ingnored;
	return recv(&ingnored, tx_address, message_buffer, message_buffer_length);
}

size_t ask_receiver_t::recv(uint8_t* rx_address, uint8_t* tx_address, void* message_buffer, size_t message_buffer_length)
{
	if (_packets_available)
	{
		--_packets_available;

		// calculate lenght of the data in the message
		size_t message_lenght = (size_t)_read_byte_from_buffer() - 7;

		size_t message_truncate = 0;
		if (message_lenght > message_buffer_length)
		{
			// truncate message to lenght of the buffer given by caller
			message_truncate = message_lenght - message_buffer_length;
			message_lenght -= message_truncate;
		}

		// read header to part
		*rx_address = _read_byte_from_buffer();

		// read header from part
		*tx_address = _read_byte_from_buffer();

		// discard header id and flag parts they are ignored by this program
		_discard_bytes_from_buffer(2);

		// read message data to  buffer given by caller
		for (uint8_t* i = (uint8_t*)message_buffer, *e = i + message_lenght; i != e; ++i)
			*i = _read_byte_from_buffer();

		// discard truncated message data and the crc it is already validated by the interrupt handler
		_discard_bytes_from_buffer(message_truncate + 2);
		return message_lenght;
	}
	return 0;
}

void ask_receiver_t::status(ask_receiver_status_t* current_status)
{
	if (_is_initialized)
	{
		current_status->rx_frequency = _rx_frequency;
		current_status->rx_pin = _rx_pin_name;
		current_status->rx_address = rx_address;
		current_status->initialized = true;
		current_status->receive_all_packets = _receive_all_packets;
		if (_rx_active)
			current_status->active = true;
		else
			current_status->active = false;
		current_status->packets_available = _packets_available;
		current_status->packets_received = _packets_received;
		current_status->packets_dropped = _packets_dropped;
		current_status->bytes_received = _bytes_received;
		current_status->bytes_dropped = _bytes_dropped;
		current_status->rx_entropy = rx_entropy;
	}
	else
	{
		current_status->rx_frequency = 0;
		current_status->rx_pin = NC;
		current_status->rx_address = ASK_RECEIVER_BROADCAST_ADDRESS;
		current_status->initialized = false;
		current_status->receive_all_packets = false;
		current_status->active = false;
		current_status->packets_available = 0;
		current_status->packets_received = 0;
		current_status->packets_dropped = 0;
		current_status->bytes_received = 0;
		current_status->bytes_dropped = 0;
		current_status->rx_entropy = ~0;
	}
}

bool ask_receiver_t::is_valid_frequency(int frequency)
{
	static const int valid_frequencies[] = { 1000, 1250, 2500, 3125 };

	// search valid frequency list for the value of frequency parameter
	bool valid_frequency = false;
	for (int i = 0, e = sizeof(valid_frequencies) / sizeof(int); !valid_frequency && i != e; ++i)
		if (frequency == valid_frequencies[i])
			valid_frequency = true;

	return valid_frequency;
}

void ask_receiver_t::_rx_interrupt_handler()
{
	uint8_t rx_sample = (uint8_t)gpio_read(&_ask_receiver->_rx_pin);

	// rx_entropy is calculated to crc32 of all samples
	uint32_t rx_crc = ~_ask_receiver->rx_entropy;
	uint32_t rx_crc_msb = ((uint32_t)rx_sample ^ rx_crc) & 1;
	_ask_receiver->rx_entropy = ~((rx_crc_msb << 31) | ((rx_crc >> 1) ^ (0x6DB88320 & (0 - rx_crc_msb))));

	// sum all samples till ramp reaches ASK_RECEIVER_RAMP_LENGTH
	_ask_receiver->_rx_integrator += rx_sample;

	if (rx_sample != _ask_receiver->_rx_last_sample)
	{
		// ramp transition
		// increase ramp by ASK_RECEIVER_RAMP_INCREMENT_RETARD if ramp < ASK_RECEIVER_RAMP_TRANSITION else by ASK_RECEIVER_RAMP_INCREMENT_ADVANCE
		if (_ask_receiver->_rx_ramp < ASK_RECEIVER_RAMP_TRANSITION)
			_ask_receiver->_rx_ramp += ASK_RECEIVER_RAMP_INCREMENT_RETARD;
		else
			_ask_receiver->_rx_ramp += ASK_RECEIVER_RAMP_INCREMENT_ADVANCE;
		_ask_receiver->_rx_last_sample = rx_sample;
	}
	else
	{
		// no ramp transition
		// increase ramp by standard increment
		_ask_receiver->_rx_ramp += ASK_RECEIVER_RAMP_INCREMENT;
	}
	if (_ask_receiver->_rx_ramp >= ASK_RECEIVER_RAMP_LENGTH)
	{
		_ask_receiver->_rx_ramp -= ASK_RECEIVER_RAMP_LENGTH;

		// received bits are shifted right and next bit is append to the end
		// next bit is calculated from sum of received samples
		_ask_receiver->_rx_bits = ((uint8_t)(_ask_receiver->_rx_integrator > (ASK_RECEIVER_SAMPLERS_PER_BIT / 2)) << 11) | (_ask_receiver->_rx_bits >> 1);
		
		// reset summed samples
		_ask_receiver->_rx_integrator = 0;

		if (_ask_receiver->_rx_active)
		{
			// if receiving a packet

			_ask_receiver->_rx_bit_count += 1;
			if (_ask_receiver->_rx_bit_count == 12)
			{
				// when receive 12 bits (2 symbols)

				_ask_receiver->_rx_bit_count = 0;

				// decode next byte from 2 received symbols
				uint8_t received_byte = (_decode_symbol((uint8_t)(_ask_receiver->_rx_bits & 0x3F)) << 4) | _decode_symbol((uint8_t)(_ask_receiver->_rx_bits >> 6));
				
				if (!_ask_receiver->_packet_received)
				{
					// first byte contains length of the packet

					if (received_byte < 7 || (size_t)received_byte > _ask_receiver->_get_buffer_free_space())
					{
						// if invalid lenght or not enough space in buffer ignore this packet
						_ask_receiver->_rx_active = 0;

						_ask_receiver->_packets_dropped++;
						if (received_byte > 6)
							_ask_receiver->_bytes_dropped += (size_t)received_byte - 7;
						return;
					}
					_ask_receiver->_packet_length = received_byte;
				}
				else if (_ask_receiver->_packet_received == 1 && !_ask_receiver->_receive_all_packets)
				{
					// ignore the packets that are not send to this receiver
					if (received_byte != ASK_RECEIVER_BROADCAST_ADDRESS && received_byte != _ask_receiver->rx_address)
					{
						_ask_receiver->_rx_active = 0;
						_ask_receiver->_erase_current_packet();
						return;
					}
				}

				_ask_receiver->_packet_received += 1;
				if (_ask_receiver->_packet_received < _ask_receiver->_packet_length - 1)
					_ask_receiver->_packet_crc = _ask_receiver->_kermit.fastCRC(_ask_receiver->_packet_crc, received_byte);// calculate crc for the packet while receiving it
				else
					_ask_receiver->_packet_received_crc = (_ask_receiver->_packet_received_crc >> 8) | ((uint16_t)received_byte << 8);// receive crc of the packet
				
				// write next byte of the packet to receivers buffer
				_ask_receiver->_write_byte_to_buffer(received_byte);

				if (_ask_receiver->_packet_received == _ask_receiver->_packet_length)
				{
					// the packet is now received
					// compare crc of the packet to calculated crc if the match the packet is valid
					// if the packet is valid it will become readable to recv function
					// if the packet is invalid it is erased

					_ask_receiver->_packet_crc = ~_ask_receiver->_packet_crc;
					if (_ask_receiver->_packet_crc == _ask_receiver->_packet_received_crc)
					{
						_ask_receiver->_packets_received++;
						_ask_receiver->_bytes_received += (size_t)_ask_receiver->_packet_length - 7;

						_ask_receiver->_packets_available += 1;
					}
					else
					{
						_ask_receiver->_erase_current_packet();

						_ask_receiver->_packets_dropped++;
						_ask_receiver->_bytes_dropped += (size_t)_ask_receiver->_packet_length - 7;
					}

					// stop receiving this packet
					_ask_receiver->_rx_active = 0;
				}
			}
		}
		else if (_ask_receiver->_rx_bits == ASK_RECEIVER_START_SYMBOL)
		{
			// if not receiving a packet and received the start symbol

			_ask_receiver->_rx_active = 1;
			_ask_receiver->_rx_bit_count = 0;
			_ask_receiver->_packet_length = 0;
			_ask_receiver->_packet_received = 0;
			_ask_receiver->_packet_crc = 0xFFFF;
			_ask_receiver->_packet_received_crc = 0;
		}
	}
}

uint8_t ask_receiver_t::_decode_symbol(uint8_t _6bit_symbol)
{
	static const uint8_t symbol_table[16] = { 0x0D, 0x0E, 0x13, 0x15, 0x16, 0x19, 0x1A, 0x1C, 0x23, 0x25, 0x26, 0x29, 0x2A, 0x2C, 0x32, 0x34 };
	for (uint8_t i = 0; i != sizeof(symbol_table); ++i)
		if (_6bit_symbol == symbol_table[i])
			return i;
	return ~0;
}

size_t ask_receiver_t::_get_buffer_free_space()
{
	size_t maximum_write_index = _rx_buffer_read_index;
	size_t write_index = _rx_buffer_write_index;
	if (maximum_write_index)
		--maximum_write_index;
	else
		maximum_write_index = ASK_RECEIVER_BUFFER_SIZE - 1;
	if (maximum_write_index < write_index)
		return ASK_RECEIVER_BUFFER_SIZE - write_index + maximum_write_index;
	else
		return maximum_write_index - write_index;
}

void ask_receiver_t::_write_byte_to_buffer(uint8_t data)
{
	// the function assumes that threre is free space in the buffer
	size_t write_index = _rx_buffer_write_index;
	_rx_buffer[write_index++] = data;
	if (write_index != ASK_RECEIVER_BUFFER_SIZE)
		_rx_buffer_write_index = write_index;
	else
		_rx_buffer_write_index = 0;
}

void ask_receiver_t::_erase_current_packet()
{
	// erases the packet currently being received from the receivers buffer
	size_t write_index = _rx_buffer_write_index;
	if (write_index < _packet_received)
		_rx_buffer_write_index = ASK_RECEIVER_BUFFER_SIZE - (_packet_received - write_index);
	else
		_rx_buffer_write_index = write_index - _packet_received;
}

uint8_t ask_receiver_t::_read_byte_from_buffer()
{
	// the function assumes that threre is data available in the buffer
	size_t read_index = _rx_buffer_read_index;
	uint8_t data = _rx_buffer[read_index++];
	if (read_index != ASK_RECEIVER_BUFFER_SIZE)
		_rx_buffer_read_index = read_index;
	else
		_rx_buffer_read_index = 0;
	return data;
}

void ask_receiver_t::_discard_bytes_from_buffer(size_t size)
{
	size_t read_index = _rx_buffer_read_index;
	if (read_index + size > ASK_RECEIVER_BUFFER_SIZE - 1)
		_rx_buffer_read_index = read_index + size - ASK_RECEIVER_BUFFER_SIZE;
	else
		_rx_buffer_read_index = read_index + size;
}