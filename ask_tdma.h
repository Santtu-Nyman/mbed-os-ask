/*
	Mbed OS ASK TDMA version 1.0.0 2018-08-09 by Santtu Nyman.
	This file is part of mbed-os-ask "https://github.com/Santtu-Nyman/mbed-os-ask".

	Description
		Some simple tdma protocol implementation made for testing ask receiver and transmitter and educational stuff.
		
	Version history
		version 1.0.0 2018-08-09
			Implementation code commented and minor updates added.
		version 0.0.1 2018-08-02
			TDMA client and base station usage documented.
		version 0.0.0 2018-08-01
			First publicly available version.
		
*/

#ifndef ASK_TDMA_H
#define ASK_TDMA_H

#define ASK_TDMA_VERSION_MAJOR 1
#define ASK_TDMA_VERSION_MINOR 0
#define ASK_TDMA_VERSION_PATCH 0

#define ASK_TDMA_IS_VERSION_ATLEAST(h, m, l) ((((unsigned long)(h) << 16) | ((unsigned long)(m) << 8) | (unsigned long)(l)) <= ((ASK_TDMA_VERSION_MAJOR << 16) | (ASK_TDMA_VERSION_MINOR << 8) | ASK_TDMA_VERSION_PATCH))


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
		ask_tdma_client_t();

		ask_tdma_client_t(PinName rx_pin, PinName tx_pin, int bit_rate);
		// the constructor call init with same parameters.

		~ask_tdma_client_t();

		int init(PinName rx_pin, PinName tx_pin, int bit_rate);
		/*
			Description
				Initializes the TDMA client object with given parameters.
				If the client is already initialized it is reinitialized with the new parameters.
				When reinitializing, client's reseved address is freed.
			Parameters
				rx_pin
					Mbed OS pin name for rx pin.
				tx_pin
					Mbed OS pin name for tx pin.
				bit_rate
					Network bit rate. This value needs to be valid for ask receiver and transmitter.
					Valid bit rates are 1000, 1250, 2500 and 3125 bit/s.
					RWS-371 receiver and TWS-BS transmitter will have high packet drop rate at bit rate of 3125 bit/s
			Return
				If the function succeeds, the return value is 0 and ASK TDMA error code on failure.
		*/

		int get_base_station_address(uint8_t* address);
		/*
			Description
				Listens for base station and gets it's address.
			Parameters
				address
					Pointer to variable that receives address of the base station.
			Return
				If the function succeeds, the return value is 0 and ASK TDMA error code on failure.
		*/

		int get_address(uint8_t* address);
		/*
			Description
				This function queries reserved address of the client.
				If the client does not have a reserved address it request new address from base station.
			Parameters
				address
					Pointer to variable that receives reserved address of the client.
			Return
				If the function succeeds, the return value is 0 and ASK TDMA error code on failure.
		*/

		int recv(int timeout, size_t message_buffer_size, void* message_buffer, uint8_t* rx_address, uint8_t* tx_address, size_t* message_received);
		/*
			Description
				Function receives next packet from the network that is send to reserved address of the client or broadcast address.
				If client does not have a reserved address only broadcasted packets are received.
				Broadcast address is 0xFF.
			Parameters
				timeout
					Specifies timeout of the function in microseconds.
					If function does not receive packet before it timeouts, it fails with error ASK_TDMA_ERROR_TIMEOUT.
					If timeout is 0 the function does not have time out, but it can return ASK_TDMA_ERROR_TIMEOUT for other reasons.
				message_buffer_size
					Size of buffer where the message is received.
				message_buffer
					Pointer to buffer where the message is received.
				rx_address
					Pointer to variable that receives packet rx address.
				tx_address
					Pointer to variable that receives packet tx address.
				message_received
					Pointer to variable that receives size of the received message.
			Return
				If the function succeeds, the return value is 0 and ASK TDMA error code on failure.
		*/

		int send(uint8_t rx_address, size_t message_size, const void* message_data, size_t* message_send);
		/*
			Description
				Function sends a packet to the network, that contains message given by function parameters.
				The packet tx address is client's reserved address if it has a reserved address else it is temporal address given by base station.
			Parameters
				rx_address
					Packet rx address.
					Broadcast address is 0xFF.
				message_size
					Size of the message to be send.
				message_data
					Pointer to buffer that contain the message to be send.
				message_send
					Pointer to variable that receives number of bytes send.
					This value is valid even if the function fails.
			Return
				If the function succeeds, the return value is 0 and ASK TDMA error code on failure.
		*/

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
/*
	Description
		Function creates and hosts a network.
	Parameters
		rx_pin
			Mbed OS pin name for rx pin.
		tx_pin
			Mbed OS pin name for tx pin.
		bit_rate
			Network bit rate. This value needs to be valid for ask receiver and transmitter.
			Valid bit rates are 1000, 1250, 2500 and 3125 bit/s.
		base_station_address
			Address for the base station.
			If this parameter is broadcast address the base station chooses a random address.
	Return
		If the function succeeds, the return value is 0 and ASK TDMA error code on failure.
*/

#endif