/*
	Mbed OS ASK TDMA version 1.0.0 2018-08-09 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".
*/

#include "ask_tdma.h"

#define ASK_TDMA_SYNCHRONIZATION_MESSAGE 0x1
#define ASK_TDMA_JOIN_MESSAGE 0x2
#define ASK_TDMA_LEAVE_MESSAGE 0x4
#define ASK_TDMA_TRANSFER_MESSAGE 0x8
#define ASK_TDMA_DATA_MESSAGE 0x0

// multiplier for calculating timeouts for operations
#define ASK_TDMA_TIMEOUT_MULTIPLIER 4

#define ASK_TDMA_MAXIMUM_FRAME_LENGTH ((132 + 26 * 12) + (132 + 1 * 12 + 6) + ((16 * 15) * (132 + 16 * 12 + 6)))

// nice value for assuming time stuff
#define ASK_TDMA_ASSUMED_MAXIMUM_FRAME_LENGTH ((132 + 26 * 12) + (132 + 1 * 12 + 6) + ((16 * ASK_TDMA_TIMEOUT_MULTIPLIER) * (132 + 16 * 12 + 6)))

static uint32_t initialize_xorshift32(ask_receiver_t* receiver)
{
	// initializes xorshift32 state from rx entropy
	ask_receiver_status_t status;
	receiver->status(&status);
	uint32_t seed = 0;
	while (!seed)
	{
		// wait for atleast 128 * receiver's samples per bit samples.
		wait_us(128000000 / status.rx_frequency);
		seed = receiver->rx_entropy ^ (uint32_t)time(0);
	}
	return seed;
}

static uint32_t xorshift32(uint32_t state)
{
	// https://en.wikipedia.org/wiki/Xorshift
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

static void discard_all_messages(ask_receiver_t* receiver)
{
	// discards all messages from the receiver
	ask_receiver_status_t status;
	receiver->status(&status);
	for (int i = 0; i != status.packets_available; ++i)
		receiver->recv(0, 0);
}

ask_tdma_client_t::ask_tdma_client_t()
{
	// just clear all state
	_base_station_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_frame_number = 0;
	_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot = 0;
	_reserved_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot_available = false;
	for (uint8_t i = 0; i != 16; ++i)
		_data_slot_lengths[i] = 0;
	_bit_rate = 0;
	_us_per_bit = 0;
	_rx_pin = NC;
	_tx_pin = NC;
}

ask_tdma_client_t::ask_tdma_client_t(PinName rx_pin, PinName tx_pin, int bit_rate)
{
	// clear all state before init
	_base_station_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_frame_number = 0;
	_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot = 0;
	_reserved_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot_available = false;
	for (uint8_t i = 0; i != 16; ++i)
		_data_slot_lengths[i] = 0;
	_bit_rate = 0;
	_us_per_bit = 0;
	_rx_pin = NC;
	_tx_pin = NC;

	// call init to initialize the client
	init(rx_pin, tx_pin, bit_rate);
}

ask_tdma_client_t::~ask_tdma_client_t()
{
	// free reserved address on destruction
	if (_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS && !join(false))
		leave(false);
}

int ask_tdma_client_t::init(PinName rx_pin, PinName tx_pin, int bit_rate)
{
	if (rx_pin == NC || tx_pin == NC || !_receiver.is_valid_frequency(bit_rate) || !_transmitter.is_valid_frequency(bit_rate))
		return ASK_TDMA_ERROR_INVALID_PARAMETER;

	// when reinitializing, free reserved address
	if (_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS && !join(false))
		leave(false);

	// clear old state
	_base_station_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_frame_number = 0;
	_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot = 0;
	_reserved_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot_available = false;
	for (uint8_t i = 0; i != 16; ++i)
		_data_slot_lengths[i] = 0;

	_bit_rate = bit_rate;
	_us_per_bit = 1000000 / _bit_rate;
	_rx_pin = rx_pin;
	_tx_pin = tx_pin;
	return 0;
}

int ask_tdma_client_t::get_base_station_address(uint8_t* address)
{
	if (_base_station_address != ASK_RECEIVER_BROADCAST_ADDRESS)
	{
		// return base station address if it is already known
		*address = _base_station_address;
		return 0;
	}

	// initialize receiver for listenin for base station
	if (!_receiver.init(_bit_rate, _rx_pin, _reserved_address))
		return ASK_TDMA_ERROR_RECEIVER_ERROR;

	// wait for frame synchronization packet
	int error = frame_synchronization(false, _reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
	_receiver.init(0, NC);
	if (error)
		return error;

	// return the tx address of frame synchronization packet
	*address = _base_station_address;
	return 0;
}

int ask_tdma_client_t::get_address(uint8_t* address)
{
	if (_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS)
	{
		// return reserved address if the client has one
		*address = _reserved_address;
		return 0;
	}

	// join to network and reserve address
	int error = join(true);
	if (error)
		return error;
	error = leave(true);

	if (_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS)
	{
		// return client's new reserved address given by base station
		*address = _reserved_address;
		return 0;
	}
	if (error)
		return error;
	return ASK_TDMA_ERROR_MALFUNCTIONING_NETWORK;
}

int ask_tdma_client_t::recv(int timeout, size_t message_buffer_size, void* message_buffer, uint8_t* rx_address, uint8_t* tx_address, size_t* message_received)
{
	// initialize receiver for receiving a transfer
	if (!_receiver.init(_bit_rate, _rx_pin, _reserved_address))
		return ASK_TDMA_ERROR_RECEIVER_ERROR;

	uint8_t data_message[16];
	uint8_t transfer_message[4];
	size_t transfer_message_size;
	size_t transfer_size;
	uint8_t receiver;
	uint8_t sender;
	size_t transfered = 0;
	uint8_t transfer_sender = ASK_RECEIVER_BROADCAST_ADDRESS;
	uint8_t transfer_receiver = ASK_RECEIVER_BROADCAST_ADDRESS;

	if (timeout)
	{
		_timer.reset();
		_timer.start();
		
		// wait for transfer packet
		for (bool waiting_for_transfer = true; waiting_for_transfer;)
		{
			if (_timer.read_us() >= timeout)
			{
				// fail the function if timeout
				_timer.stop();
				_receiver.init(0, NC);
				*rx_address = transfer_receiver;
				*tx_address = transfer_sender;
				*message_received = transfered;
				return ASK_TDMA_ERROR_TIMEOUT;
			}
			transfer_message_size = _receiver.recv(&transfer_receiver, &transfer_sender, transfer_message, 4);
			if (transfer_message_size == 4 && (transfer_message[0] & 0xF) == ASK_TDMA_TRANSFER_MESSAGE)
			{
				// transfer packet received
				uint32_t tranfer_raw_size = (uint32_t)transfer_message[1] | ((uint32_t)transfer_message[2] << 8) | ((uint32_t)transfer_message[3] << 16);
				if (tranfer_raw_size > SIZE_MAX)
				{
					// fail if size of message being transfered is greater than SIZE_MAX
					_receiver.init(0, NC);
					*rx_address = transfer_receiver;
					*tx_address = transfer_sender;
					*message_received = transfered;
					return ASK_TDMA_ERROR_TRANSFER_TOO_LARGE;
				}
				transfer_size = (size_t)tranfer_raw_size;
				waiting_for_transfer = false;
			}
		}

		// begin to read data packets of the transfer
		bool not_last = true;
		while (not_last && transfered != transfer_size && _timer.read_us() < timeout)
		{
			size_t message_size = _receiver.recv(&receiver, &sender, data_message, 16);
			if (message_size && (data_message[0] & 0xF) == ASK_TDMA_DATA_MESSAGE && receiver == transfer_receiver && sender == transfer_sender)
			{
				// data packet received add it's data to message buffer
				--message_size;
				not_last = (data_message[0] & 0x10) != 0;
				if (message_size > transfer_size - transfered)
					message_size = transfer_size - transfered;
				for (uint8_t* i = data_message + 1, *e = i + message_size; i != e; ++i, message_buffer = (void*)((uintptr_t)message_buffer + 1))
					*(uint8_t*)message_buffer = *i;
				transfered += message_size;
			}
		}
		bool error_timeout = (not_last && transfered != transfer_size) ? _timer.read_us() >= timeout : false;
		_timer.stop();
		_receiver.init(0, NC);
		*rx_address = transfer_receiver;
		*tx_address = transfer_sender;
		*message_received = transfered;
		if (transfered != transfer_size)
		{
			if (error_timeout)
				return ASK_TDMA_ERROR_TIMEOUT;
			else if (!not_last)
				return ASK_TDMA_ERROR_PACKETS_LOST;
			else
				return ASK_TDMA_ERROR_INSUFFICIENT_BUFFER;
		}
		return 0;
	}
	else
	{
		// wait for transfer packet
		for (bool waiting_for_transfer = true; waiting_for_transfer;)
		{
			transfer_message_size = _receiver.recv(&transfer_receiver, &transfer_sender, transfer_message, 4);
			if (transfer_message_size == 4 && (transfer_message[0] & 0xF) == ASK_TDMA_TRANSFER_MESSAGE)
			{
				// transfer packet received
				uint32_t tranfer_raw_size = (uint32_t)transfer_message[1] | ((uint32_t)transfer_message[2] << 8) | ((uint32_t)transfer_message[3] << 16);
				if (tranfer_raw_size > SIZE_MAX)
				{
					// fail if size of message being transfered is greater than SIZE_MAX
					_receiver.init(0, NC);
					*rx_address = transfer_receiver;
					*tx_address = transfer_sender;
					*message_received = transfered;
					return ASK_TDMA_ERROR_TRANSFER_TOO_LARGE;
				}
				transfer_size = (size_t)tranfer_raw_size;
				waiting_for_transfer = false;
			}
		}

		// begin to read data packets of the transfer
		bool not_last = true;
		while (not_last && transfered != transfer_size)
		{
			size_t message_size = _receiver.recv(&receiver, &sender, data_message, 16);
			if (message_size && (data_message[0] & 0xF) == ASK_TDMA_DATA_MESSAGE && receiver == transfer_receiver && sender == transfer_sender)
			{
				// data packet received add it's data to message buffer
				--message_size;
				not_last = (data_message[0] & 0x10) != 0;
				if (message_size > transfer_size - transfered)
					message_size = transfer_size - transfered;
				for (uint8_t* i = data_message + 1, *e = i + message_size; i != e; ++i, message_buffer = (void*)((uintptr_t)message_buffer + 1))
					*(uint8_t*)message_buffer = *i;
				transfered += message_size;
			}
		}
		_receiver.init(0, NC);
		*rx_address = transfer_receiver;
		*tx_address = transfer_sender;
		*message_received = transfered;
		if (transfered != transfer_size)
		{
			if (!not_last)
				return ASK_TDMA_ERROR_PACKETS_LOST;
			else
				return ASK_TDMA_ERROR_INSUFFICIENT_BUFFER;
		}
		return 0;
	}
}

int ask_tdma_client_t::send(uint8_t rx_address, size_t message_size, const void* message_data, size_t* message_send)
{
	// join to network for transfering a massage
	int error = join(_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
	if (error)
		return error;

	uint8_t data_message[16];

	uint8_t data_slot_available = wait_for_data_slot();

	if (data_slot_available)
	{
		// if joining to network succeeds, data slot should be available
		// send transfer packet to begin the transfer
		uint8_t transfer_message[4] = { (uint8_t)(ASK_TDMA_TRANSFER_MESSAGE | (_frame_number << 5)), (uint8_t)message_size, (uint8_t)(message_size >> 8), (uint8_t)(message_size >> 16) };
		_transmitter.send(rx_address, transfer_message, 4);
		--data_slot_available;
		wait_us(330 * _us_per_bit);
	}
	else
	{
		// this code should not be possible to reach if thins are working correctly
		leave(_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
		*message_send = 0;
		return ASK_TDMA_ERROR_MALFUNCTIONING_NETWORK;
	}

	size_t message_remaining = message_size;

	// loop until the message is sent
	while (message_remaining)
	{
		// send data packets of the current frame
		for (uint8_t i = 0; message_remaining && i != data_slot_available; ++i)
		{
			size_t packet_message_size = (message_remaining < 16) ? message_remaining : 15;
			message_remaining -= packet_message_size;
			data_message[0] = (uint8_t)(ASK_TDMA_DATA_MESSAGE | ((message_remaining ? 1 : 0) << 4) | (_frame_number << 5));
			for (uint8_t* i = data_message + 1, *e = i + packet_message_size; i != e; ++i, message_data = (const void*)((uintptr_t)message_data + 1))
				*i = *(const uint8_t*)message_data;
			_transmitter.send(rx_address, data_message, 1 + packet_message_size);
			wait_us(330 * _us_per_bit);
		}

		// if more data packet to send wait for next frame
		if (message_remaining)
		{
			error = frame_synchronization(false, _reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
			if (error)
			{
				leave(_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
				*message_send = message_size - message_remaining;
				return error;
			}
			data_slot_available = wait_for_data_slot();
			if (!data_slot_available)
			{
				// this code should not be possible to reach if thins are working correctly
				leave(_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
				*message_send = message_size - message_remaining;
				return ASK_TDMA_ERROR_MALFUNCTIONING_NETWORK;
			}
		}
	}

	// leave network when, transfer finished
	leave(_reserved_address != ASK_RECEIVER_BROADCAST_ADDRESS);
	*message_send = message_size;
	return 0;
}

int ask_tdma_client_t::frame_synchronization(bool join, bool reserve_address)
{
	uint8_t data[34];
	uint8_t size;
	uint8_t receiver_address;
	uint8_t sender_address;

	_timer.reset();
	_timer.start();

	// wait for frame synchronization packet
	while (_timer.read_us() < ASK_TDMA_TIMEOUT_MULTIPLIER * ASK_TDMA_ASSUMED_MAXIMUM_FRAME_LENGTH * _us_per_bit)
	{
		size = (uint8_t)_receiver.recv(&receiver_address, &sender_address, data, 34);

		if (receiver_address == ASK_RECEIVER_BROADCAST_ADDRESS && size > 1 && (data[0] & 0xF) == ASK_TDMA_SYNCHRONIZATION_MESSAGE)
		{
			// frame synchronization packet received

			// discard old trash from the receiver
			discard_all_messages(&_receiver);

			_base_station_address = sender_address;

			_frame_number = data[0] >> 5;

			uint8_t data_slot_count = data[1] & 0x1F;

			uint8_t rename_count = (size - 2) >> 1;

			for (uint8_t i = 0; i != rename_count; ++i)
				_data_slot_lengths[data[2 + (i << 1) + 1] & 0xF] = data[2 + (i << 1) + 1] >> 4;

			if (join)
			{
				// when joining network test if base station send join bit
				if ((data[0] & 0x10) && rename_count)
				{
					_temporal_address = data[2];
					_data_slot = data[3] & 0xF;
					if (reserve_address)
						_reserved_address = _temporal_address;
					_receiver.rx_address = _temporal_address;
					_transmitter.tx_address = _temporal_address;
				}
			}
			else
			{
				if (_temporal_address != ASK_RECEIVER_BROADCAST_ADDRESS)
				{
					// when being connected to network, the cliend needs to find it's data slot and length of the slot

					bool data_slot_removed = false;
					for (uint8_t i = 0; i != rename_count && !data_slot_removed; ++i)
						if ((data[2 + (i << 1) + 1] & 0xF) == _data_slot)
							data_slot_removed = true;

					if (data_slot_removed)
					{
						for (uint8_t i = 0; i != rename_count && data_slot_removed; ++i)
							if (data[2 + (i << 1)] == _temporal_address)
							{
								_data_slot = data[2 + (i << 1) + 1] & 0xF;
								data_slot_removed = false;
							}

						if (data_slot_removed)
						{
							_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
							if (!reserve_address)
								_reserved_address = ASK_RECEIVER_BROADCAST_ADDRESS;
							_receiver.rx_address = _reserved_address;
							_transmitter.tx_address = _reserved_address;
						}
					}

					// assume that the client has been disconnected from the network, if this invalid state is reached
					if ((data[0] & 0x10) && rename_count && data[2] == _temporal_address)
						_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
				}
			}
			_timer.stop();

			if (_temporal_address != ASK_RECEIVER_BROADCAST_ADDRESS && (_data_slot >= data_slot_count || !_data_slot_lengths[_data_slot]))
			{
				// the client has no time allocated to transmit data. it has been disconnected from the network
				_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
				if (!reserve_address)
					_reserved_address = ASK_RECEIVER_BROADCAST_ADDRESS;
				_receiver.rx_address = _reserved_address;
				_transmitter.tx_address = _reserved_address;
			}

			if (_temporal_address != ASK_RECEIVER_BROADCAST_ADDRESS)
				_data_slot_available = true;

			return 0;
		}
	}
	_timer.stop();

	_base_station_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot_available = false;
	_receiver.rx_address = _reserved_address;
	_transmitter.tx_address = _reserved_address;

	return ASK_TDMA_ERROR_TIMEOUT;
}

uint8_t ask_tdma_client_t::wait_for_data_slot()
{
	// test if current frames data slot is used
	if (_data_slot_available)
	{
		_data_slot_available = false;
		int data_slot_lengths = 0;
		for (uint8_t i = 0; i != _data_slot; ++i)
			data_slot_lengths += (int)_data_slot_lengths[i];

		int wait_bits = (132 + 1 * 12 + 6) + ((int)data_slot_lengths * (132 + 16 * 12 + 6));

		if ((132 + 26 * 12) + wait_bits > ASK_TDMA_ASSUMED_MAXIMUM_FRAME_LENGTH)
		{
			// some thing went wrong
			return 0;
		}

		// wait for the time of client's data slot

		wait_us(wait_bits * _us_per_bit);

		// return the length of the data slot
		return _data_slot_lengths[_data_slot];
	}
	else
		return 0;
}

int ask_tdma_client_t::join(bool reserve_address)
{
	if (!_receiver.init(_bit_rate, _rx_pin, _reserved_address))
		return ASK_TDMA_ERROR_RECEIVER_ERROR;
	if (!_transmitter.init(_bit_rate, _tx_pin, _reserved_address))
	{
		_receiver.init(0, NC);
		return ASK_TDMA_ERROR_TRANSMITTER_ERROR;
	}

	// initialize xorshift32 state for random join change calculations
	uint32_t xorshift32_state = initialize_xorshift32(&_receiver);

	_timer.reset();
	_timer.start();

	// discard old trash from the receiver
	discard_all_messages(&_receiver);

	for (int join_requests_failed = 0, join_request_send = 0; _timer.read_us() < ASK_TDMA_TIMEOUT_MULTIPLIER * ASK_TDMA_ASSUMED_MAXIMUM_FRAME_LENGTH * _us_per_bit;)
	{
		// wait for frame synchronization packet and test if join request was successful if it was send
		int error = frame_synchronization(join_request_send != 0, reserve_address);
		if (!error)
		{
			if (join_request_send)
			{
				if (_temporal_address != ASK_RECEIVER_BROADCAST_ADDRESS)
				{
					_timer.stop();
					return 0;
				}
				else if (join_requests_failed < 0xF)
					++join_requests_failed;
				join_request_send = 0;
			}

			// try joining with random change to prevent join packet collisions
			if (join_requests_failed == 0xF)
			{
				xorshift32_state = xorshift32(xorshift32_state);
				if (!(xorshift32_state & 0xF))
					xorshift32_state = _receiver.rx_entropy ^ (uint32_t)time(0);
			}
			if (!xorshift32_state)
				xorshift32_state = _receiver.rx_entropy ^ (uint32_t)time(0);
			xorshift32_state = xorshift32(xorshift32_state);
			if ((16 - join_requests_failed) > (int)(xorshift32_state & 0xF))
			{
				uint8_t join_message = ASK_TDMA_JOIN_MESSAGE | ((uint8_t)(reserve_address ? 1 : 0) << 4) | (_frame_number << 5);
				_transmitter.send(_base_station_address, &join_message, 1);
				join_request_send = 1;
			}
		}
		else
		{
			_timer.stop();
			_receiver.init(0, NC);
			_transmitter.init(0, NC);
			return error;
		}
	}

	_timer.stop();
	_receiver.init(0, NC);
	_transmitter.init(0, NC);
	return ASK_TDMA_ERROR_TIMEOUT;
}

int ask_tdma_client_t::leave(bool reserve_address)
{
	_timer.reset();
	_timer.start();

	int error = 0;

	while (!error && _timer.read_us() < ASK_TDMA_TIMEOUT_MULTIPLIER * ASK_TDMA_ASSUMED_MAXIMUM_FRAME_LENGTH * _us_per_bit)
	{
		// send leave packet to base station on next data slot
		if (wait_for_data_slot())
		{
			uint8_t leave_message = ASK_TDMA_LEAVE_MESSAGE | ((uint8_t)(reserve_address ? 1 : 0) << 4) | (_frame_number << 5);
			_transmitter.send(_base_station_address, &leave_message, 1);
		}

		// wait for frame synchronization packet
		error = frame_synchronization(false, reserve_address);
		if (!error && _temporal_address == ASK_RECEIVER_BROADCAST_ADDRESS)
		{
			_timer.stop();
			_receiver.init(0, NC);
			_transmitter.init(0, NC);
			return 0;
		}
	}

	_timer.stop();
	_receiver.init(0, NC);
	_transmitter.init(0, NC);
	_base_station_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_temporal_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	_data_slot = 0;
	_data_slot_available = false;

	if (error)
		return error;

	return ASK_TDMA_ERROR_TIMEOUT;
}

typedef struct ask_tdma_data_slot_t
{
	uint8_t address;
	uint8_t length;
	int8_t usage;
	uint8_t frame_usage;
	bool transfer_ended;
	bool keep_address_reserved;
} ask_tdma_data_slot_t;

typedef struct ask_tdma_server_t
{
	uint8_t data_slot_count;
	uint8_t frame_number;
	ask_tdma_data_slot_t data_slots[16];
	uint8_t message_buffer[34];
	uint8_t reserved_addresses[32];
	uint8_t rename_count;
	uint8_t renames[32];
	int bit_rate;
	int us_per_bit;
	uint32_t xorshift32_state;
	bool run;
	ask_receiver_t receiver;
	ask_transmitter_t transmitter;
} ask_tdma_server_t;

static void reserve_address(uint8_t* reserved_addresses, uint8_t address)
{
	reserved_addresses[address >> 3] |= (1 << (address & 7));
}

static bool is_address_reserved(const uint8_t* reserved_addresses, uint8_t address)
{
	return (reserved_addresses[address >> 3] & (1 << (address & 7))) != 0;
}

static void free_address(uint8_t* reserved_addresses, uint8_t address)
{
	reserved_addresses[address >> 3] &= ~(1 << (address & 7));
}

static bool get_free_address(const uint8_t* reserved_addresses, uint8_t* address, uint32_t* xorshift32_state)
{
	uint32_t new_xorshift32_state = xorshift32(*xorshift32_state);
	*xorshift32_state = new_xorshift32_state;
	for (uint8_t m = (uint8_t)new_xorshift32_state, i = 0; i != 32; ++i)
		if (reserved_addresses[(i + (m >> 3)) & 0x1F] != 0xFF)
			for (uint8_t j = 0; j != 8; j++)
				if (!(reserved_addresses[(i + (m >> 3)) & 0x1F] & (1 << ((j + (m & 7)) & 0x7))))
				{
					*address = (((i + (m >> 3)) & 0x1F) << 3) | ((j + (m & 7)) & 0x7);
					return true;
				}
	*address = ASK_RECEIVER_BROADCAST_ADDRESS;
	return false;
}

static void process_frame_requests(ask_tdma_server_t* server, uint8_t synchronization_message_size, ask_tdma_data_slot_t* join_request)
{
	for (uint8_t i = 0; i != server->data_slot_count; ++i)
	{
		server->data_slots[i].frame_usage = 0;
		server->data_slots[i].transfer_ended = false;
	}
	int join_request_count = 0;
	bool keep_join_address_reserved = false;
	uint8_t join_request_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	uint8_t receiver_address = ASK_RECEIVER_BROADCAST_ADDRESS;
	uint8_t sender_address = ASK_RECEIVER_BROADCAST_ADDRESS;

	// calculate length of current frame
	int frame_length = (132 + (int)synchronization_message_size * 12) + 150;
	for (uint8_t i = 0; i != server->data_slot_count; ++i)
		frame_length += (int)server->data_slots[i].length * 330;
	frame_length *= server->us_per_bit;

	Timer timer;
	timer.start();
	while (timer.read_us() < frame_length)
	{
		// here base station processes all messages send in the current frame

		size_t message_size = server->receiver.recv(&receiver_address, &sender_address, server->message_buffer, 34);
		if (message_size && sender_address != server->receiver.rx_address)
		{
			bool data_slot_address = false;
			if (sender_address != ASK_RECEIVER_BROADCAST_ADDRESS)
				for (uint8_t i = 0; !data_slot_address && i != server->data_slot_count; ++i)
					if (server->data_slots[i].address == sender_address)
					{
						// process message from client that is connected to the network
						server->data_slots[i].frame_usage++;
						if (receiver_address == server->receiver.rx_address && message_size && (server->message_buffer[0] & 0xF) == ASK_TDMA_LEAVE_MESSAGE)
						{
							server->data_slots[i].usage = -128;
							server->data_slots[i].keep_address_reserved = (server->message_buffer[0] & 0x10) != 0;
						}
						else if (message_size && (server->message_buffer[0] & 0xF) == ASK_TDMA_DATA_MESSAGE && !(server->message_buffer[0] & 0x10))
							server->data_slots[i].transfer_ended = true;
						data_slot_address = true;
					}
			if (!data_slot_address && message_size && (server->message_buffer[0] & 0xF) == ASK_TDMA_JOIN_MESSAGE && (sender_address == ASK_RECEIVER_BROADCAST_ADDRESS || is_address_reserved(server->reserved_addresses, sender_address)))
			{
				// process message from client that is trying to connect to the network
				++join_request_count;
				keep_join_address_reserved = (server->message_buffer[0] & 0x10) != 0;
				join_request_address = sender_address;
			}
		}
	}
	timer.stop();
	for (uint8_t i = 0; i != server->data_slot_count; ++i)
		if (server->data_slots[i].address != ASK_RECEIVER_BROADCAST_ADDRESS)
		{
			// update client data slot usage information.
			if (server->data_slots[i].frame_usage && server->data_slots[i].frame_usage <= server->data_slots[i].length && server->data_slots[i].usage != -128)
			{
				if (server->data_slots[i].usage != 127)
					server->data_slots[i].usage++;
			}
			else if (!server->data_slots[i].frame_usage && server->data_slots[i].usage != -128)
				server->data_slots[i].usage--;
			else
				server->data_slots[i].usage = -128;
		}
	if (join_request_count == 1)
	{
		// if new client is trying to join the network accept the join request
		if (join_request_address == ASK_RECEIVER_BROADCAST_ADDRESS)
		{
			if (!server->xorshift32_state)
				server->xorshift32_state = server->receiver.rx_entropy ^ (uint32_t)time(0);
			if (get_free_address(server->reserved_addresses, &join_request->address, &server->xorshift32_state))
			{
				reserve_address(server->reserved_addresses, join_request->address);
				join_request->length = 1;
				join_request->usage = 0;
				join_request->frame_usage = 0;
				join_request->transfer_ended = false;
				join_request->keep_address_reserved = keep_join_address_reserved;
			}
			else
			{
				join_request->address = ASK_RECEIVER_BROADCAST_ADDRESS;
				join_request->length = 0;
				join_request->usage = -128;
				join_request->frame_usage = 0;
				join_request->transfer_ended = false;
				join_request->keep_address_reserved = false;
			}
		}
		else
		{
			join_request->address = join_request_address;
			join_request->length = 1;
			join_request->usage = 0;
			join_request->frame_usage = 0;
			join_request->transfer_ended = false;
			join_request->keep_address_reserved = keep_join_address_reserved;
		}
	}
	else
	{
		join_request->address = ASK_RECEIVER_BROADCAST_ADDRESS;
		join_request->length = 0;
		join_request->usage = -128;
		join_request->frame_usage = 0;
		join_request->transfer_ended = false;
		join_request->keep_address_reserved = false;
	}
	discard_all_messages(&server->receiver);
}

static bool process_frame_renaming(ask_tdma_server_t* server, const ask_tdma_data_slot_t* join_request)
{
	bool join_request_accepted = false;
	server->rename_count = 0;

	// remove idle clients from the network
	for (uint8_t i = 0; i != server->data_slot_count; ++i)
		if (server->data_slots[i].usage < -1)
		{
			if (!server->data_slots[i].keep_address_reserved)
				free_address(server->reserved_addresses, server->data_slots[i].address);
			if (join_request)
			{
				// if new client is joining give it data slot of some idle client
				server->data_slots[i] = *join_request;
				join_request = 0;
				join_request_accepted = true;
			}
			else
			{
				server->data_slots[i].address = ASK_RECEIVER_BROADCAST_ADDRESS;
				server->data_slots[i].length = 0;
				server->data_slots[i].usage = 0;
				server->data_slots[i].transfer_ended = false;
				server->data_slots[i].keep_address_reserved = false;
			}
			server->renames[(server->rename_count << 1)] = server->data_slots[i].address;
			server->renames[(server->rename_count << 1) + 1] = i | (server->data_slots[i].length << 4);
			server->rename_count++;
		}

	if (join_request)
	{
		// find unused data slot for the new client
		for (uint8_t i = 0; i != server->data_slot_count; ++i)
			if (!server->data_slots[i].length)
			{
				server->data_slots[i] = *join_request;
				join_request = 0;
				join_request_accepted = true;
				server->renames[(server->rename_count << 1)] = server->data_slots[i].address;
				server->renames[(server->rename_count << 1) + 1] = i | (server->data_slots[i].length << 4);
				server->rename_count++;
				server->data_slot_count++;
			}
		if (join_request)
		{
			if (server->data_slot_count != 16)
			{
				server->data_slots[server->data_slot_count] = *join_request;
				join_request = 0;
				join_request_accepted = true;
				server->renames[(server->rename_count << 1)] = server->data_slots[server->data_slot_count].address;
				server->renames[(server->rename_count << 1) + 1] = server->data_slot_count | (server->data_slots[server->data_slot_count].length << 4);
				server->rename_count++;
				server->data_slot_count++;
			}
			else
			{
				// if new client can't use data slot of idle client and all 16 data slot are used find some client to remove
				uint8_t h = 0;
				for (uint8_t i = 1; i != server->data_slot_count; ++i)
					if (server->data_slots[i].usage > server->data_slots[h].usage)
						h = i;
				if (!server->data_slots[h].keep_address_reserved)
					free_address(server->reserved_addresses, server->data_slots[h].address);
				server->data_slots[h] = *join_request;
				join_request = 0;
				join_request_accepted = true;
				server->renames[(server->rename_count << 1)] = server->data_slots[h].address;
				server->renames[(server->rename_count << 1) + 1] = h | (server->data_slots[h].length << 4);
				server->rename_count++;
			}
		}
	}

	// if no renaming optimize data slot lengths by shortening data slot of probably leaving clients
	if (!server->rename_count)
		for (uint8_t i = 0; i != server->data_slot_count; ++i)
			if (server->data_slots[i].length > 1 && server->data_slots[i].transfer_ended)
			{
				server->data_slots[i].length = 1;
				server->renames[(server->rename_count << 1)] = server->data_slots[i].address;
				server->renames[(server->rename_count << 1) + 1] = i | (server->data_slots[i].length << 4);
				server->rename_count++;
			}

	// if no renaming optimize data slot order by removing empty slot from between used slots
	if (!server->rename_count)
		for (uint8_t i = 0; i != server->data_slot_count && !server->rename_count; ++i)
			if (!server->data_slots[i].length)
				for (uint8_t j = i + 1; j != server->data_slot_count; ++j)
					if (server->data_slots[j].length)
					{
						server->data_slots[i] = server->data_slots[j];
						server->data_slots[j].address = ASK_RECEIVER_BROADCAST_ADDRESS;
						server->data_slots[j].length = 0;
						server->data_slots[j].usage = 0;
						server->data_slots[j].transfer_ended = false;
						server->data_slots[j].keep_address_reserved = false;
						server->renames[(server->rename_count << 1)] = server->data_slots[i].address;
						server->renames[(server->rename_count << 1) + 1] = i | (server->data_slots[i].length << 4);
						server->rename_count++;
						server->renames[(server->rename_count << 1)] = server->data_slots[j].address;
						server->renames[(server->rename_count << 1) + 1] = j | (server->data_slots[j].length << 4);
						server->rename_count++;
					}

	// if no renaming optimize data slot lengths by giving longer data slots to clients that use them
	if (!server->rename_count)
		for (uint8_t i = 0; i != server->data_slot_count && !server->rename_count; ++i)
			if (server->data_slots[i].usage && !server->data_slots[i].transfer_ended && server->data_slots[i].length < 4)
			{
				server->data_slots[i].length++;
				server->renames[(server->rename_count << 1)] = server->data_slots[i].address;
				server->renames[(server->rename_count << 1) + 1] = i | (server->data_slots[i].length << 4);
				server->rename_count++;
			}

	// find last used data slot
	server->data_slot_count = 0;
	for (uint8_t i = 16; i-- && !server->data_slot_count;)
		if (server->data_slots[i].length)
			server->data_slot_count = i + 1;

	// when new client joins it needs to know lengths of other data slot so it can calculate time for it's data slot
	if (join_request_accepted)
		for (uint8_t i = 0; i != server->data_slot_count; ++i)
		{
			bool renamed = false;
			for (uint8_t j = 0; !renamed && j != server->rename_count; ++j)
				if (server->renames[j << 1] == server->data_slots[i].address)
					renamed = true;
			if (!renamed)
			{
				server->renames[(server->rename_count << 1)] = server->data_slots[i].address;
				server->renames[(server->rename_count << 1) + 1] = i | (server->data_slots[i].length << 4);
				server->rename_count++;
			}
		}

	return join_request_accepted;
}

static uint8_t write_frame_synchronization_message(ask_tdma_server_t* server, bool join_request_accepted)
{
	server->message_buffer[0] = ASK_TDMA_SYNCHRONIZATION_MESSAGE | (join_request_accepted ? 0x10 : 0) | (server->frame_number << 5);
	server->message_buffer[1] = server->data_slot_count;
	for (uint8_t i = 0, e = server->rename_count << 1; i != e; ++i)
		server->message_buffer[2 + i] = server->renames[i];
	return 2 + (server->rename_count << 1);
}

static int start_server(PinName rx_pin, PinName tx_pin, int bit_rate, uint8_t base_station_address, ask_tdma_server_t* server)
{
	// test if parameters are correct for starting a base station
	if (rx_pin == NC || tx_pin == NC || !server->receiver.is_valid_frequency(bit_rate) || !server->transmitter.is_valid_frequency(bit_rate))
		return ASK_TDMA_ERROR_INVALID_PARAMETER;

	// start base station ASK receiver and transmitter
	if (!server->receiver.init(bit_rate, rx_pin, ASK_RECEIVER_BROADCAST_ADDRESS, true))
		return ASK_TDMA_ERROR_RECEIVER_ERROR;
	if (!server->transmitter.init(bit_rate, tx_pin))
	{
		server->receiver.init(0, NC);
		return ASK_TDMA_ERROR_TRANSMITTER_ERROR;
	}

	// free all addresses of the new network
	for (int i = 0; i != 32; ++i)
		server->reserved_addresses[i] = 0;

	// reserve the broadcast address
	reserve_address(server->reserved_addresses, ASK_RECEIVER_BROADCAST_ADDRESS);

	// initialize base station xorshift32 state
	server->xorshift32_state = initialize_xorshift32(&server->receiver);

	// get address for base station if it was not provided by the caller
	if (base_station_address == ASK_RECEIVER_BROADCAST_ADDRESS)
		if (!get_free_address(server->reserved_addresses, &base_station_address, &server->xorshift32_state))
		{
			server->receiver.init(0, NC);
			server->transmitter.init(0, NC);
			return ASK_TDMA_ERROR_ADDRESS_ALREADY_RESERVED;
		}
	reserve_address(server->reserved_addresses, base_station_address);

	// clear all data slot information
	for (uint8_t i = 0; i != 16; ++i)
	{
		server->data_slots[i].address = ASK_RECEIVER_BROADCAST_ADDRESS;
		server->data_slots[i].length = 0;
		server->data_slots[i].usage = 0;
		server->data_slots[i].frame_usage = 0;
		server->data_slots[i].keep_address_reserved = false;
	}

	// set base station initial state
	server->data_slot_count = 0;
	server->frame_number = 0;
	server->rename_count = 0;
	server->bit_rate = bit_rate;
	server->us_per_bit = 1000000 / server->bit_rate;
	server->run = true;
	server->receiver.rx_address = base_station_address;
	server->transmitter.tx_address = base_station_address;

	return 0;
}

int ask_tdma_host_network(PinName rx_pin, PinName tx_pin, int bit_rate, uint8_t base_station_address)
{
	ask_tdma_server_t server;
	int error = start_server(rx_pin, tx_pin, bit_rate, base_station_address, &server);
	if (error)
		return error;

	ask_tdma_data_slot_t join_request;

	for (uint8_t synchronization_message_size = 0; !error && server.run;)
	{
		// process the messages send by clients in the frame
		process_frame_requests(&server, synchronization_message_size, &join_request);

		// do renaming of data slot for next frame
		bool join_request_accepted = process_frame_renaming(&server, (join_request.usage < 0) ? 0 : &join_request);

		// write next synchronization message
		synchronization_message_size = write_frame_synchronization_message(&server, join_request_accepted);
		server.frame_number++;

		// begin new frame by sendin a synchronization message
		server.transmitter.send(ASK_RECEIVER_BROADCAST_ADDRESS, server.message_buffer, (size_t)synchronization_message_size);
	}
	// exit network hosting if error occurs

	// shutdown base station ASK receiver and transmitter
	server.receiver.init(0, NC);
	server.transmitter.init(0, NC);

	return error;
}