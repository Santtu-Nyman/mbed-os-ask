/*
	Mbed OS ASK TDMA version 0.0.0 2018-08-01 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".

	Description
		Some simple tdma protocol implementation made for testing ask receiver and transmitter and other educational purposes.
		
*/

#ifndef ASK_TDMA_H
#define ASK_TDMA_H

#include "mbed.h"
#include "ask_receiver.h"
#include "ask_transmitter.h"

#define ASK_TDMA_ERROR_NO_ERROR 0
#define ASK_TDMA_ERROR_INVALID_PARAMETER 1
#define ASK_TDMA_ERROR_RECEIVER_ERROR 2
#define ASK_TDMA_ERROR_TRANSMITTER_ERROR 3
#define ASK_TDMA_ERROR_ALL_ADDRESSES_RESERVED 4
#define ASK_TDMA_ERROR_NO_RESPONSE 5
#define ASK_TDMA_ERROR_TIMEOUT 6
#define ASK_TDMA_ERROR_ADDRESS_ALREADY_RESERVED 7
#define ASK_TDMA_ERROR_NO_NETWORK 8
#define ASK_TDMA_ERROR_NETWORK_ALREADY_EXISTS 9
#define ASK_TDMA_ERROR_INVALID_BIT_RATE 10
#define ASK_TDMA_ERROR_INVALID_ADDRESS 11
#define ASK_TDMA_ERROR_MALFUNCTIONING_NETWORK 12
#define ASK_TDMA_ERROR_TRANSFER_TOO_LARGE 13
#define ASK_TDMA_ERROR_INSUFFICIENT_BUFFER 14
#define ASK_TDMA_ERROR_PACKETS_LOST 15

class ask_tdma_client_t
{
	public:
		// usage documentation will be added later
		ask_tdma_client_t();
		ask_tdma_client_t(PinName rx_pin, PinName tx_pin, int bit_rate);
		~ask_tdma_client_t();
		int init(PinName rx_pin, PinName tx_pin, int bit_rate);
		int get_base_station_address(uint8_t* address);
		int get_address(uint8_t* address);
		int recv(int timeout, size_t message_buffer_size, void* message_buffer, uint8_t* rx_address, uint8_t* tx_address, size_t* message_received);
		int send(uint8_t rx_address, size_t message_size, const void* message_data, size_t* message_send);
	private:
		int frame_synchronization(bool join, bool reserve_address);
		uint8_t wait_for_data_slot();
		int join(bool reserve_address);
		int leave(bool reserve_address);

		uint8_t _base_station_address;
		uint8_t _frame_number;
		uint8_t _temporal_address;
		uint8_t _data_slot;
		uint8_t _reserved_address;
		bool _data_slot_available;
		uint8_t _data_slot_lengths[16];

		int _bit_rate;
		int _us_per_bit;
		PinName _rx_pin;
		PinName _tx_pin;

		Timer _timer;
		ask_receiver_t _receiver;
		ask_transmitter_t _transmitter;

		// No copying object of this type!
		ask_tdma_client_t(const ask_tdma_client_t&);
		ask_tdma_client_t& operator=(const ask_tdma_client_t&);
};

int ask_tdma_host_network(PinName rx_pin, PinName tx_pin, int bit_rate, uint8_t base_station_address);

#endif