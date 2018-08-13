#include "mbed.h"
#include "ask_tdma.h"

size_t kp2018_spam_bot_string_length(const char* string)
{
	const char* string_read = string;
	while (*string_read)
		++string_read;
	return (size_t)((uintptr_t)string_read - (uintptr_t)string);
}

size_t kp2018_spam_bot_write_uint32(char* buffer, size_t buffer_length, uint32_t integer)
{
	size_t written = 0;
	do
	{
		for (char* move = buffer + written++; move != buffer; --move)
			*move = *(move - 1);
		*buffer = '0' + (integer % 10);
		integer /= 10;
	} while (written != buffer_length && integer);
	if (written != buffer_length)
	{
		*(buffer + written) = 0;
		return written;
	}
	else
		return 0;
}

char kp2018_spam_bot_buffer[0x100];

void kp2018_spam_bot()
{
	DigitalOut led1(LED1, 0);

	const int f = 1000;
	const char* messages[16] = {
		"hello world", "hello world!", "hello world!!", "hello world!!!", "hello world!!!!",
		"test message 123456789", "TYPE 123 IN THE CHAT TO ENTER THE DOOR GIVEAWAY!", "123",
		"hello hello", "hello hell", "Santtu was here", "k", "xyzw", "test message", "",
		"The farad (symbol: F) is the SI derived unit of electrical capacitance, the ability of a body to store an electrical charge. It is named after the English physicist Michael Faraday. covfefe" };

	ask_tdma_client_t client;
	int error = client.init(D4, D2, f);

	uint8_t address = 0;
	for (uint32_t i = 0;; ++i)
	{
		//led1 = 1;
		//wait_us(500000);
		//led1 = 0;
		//wait_us(500000);

		error = client.get_address(&address);

		//pc.printf("get_address %i\n", error);

		kp2018_spam_bot_buffer[0] = '[';
		size_t message_length = kp2018_spam_bot_write_uint32(kp2018_spam_bot_buffer + 1, sizeof(kp2018_spam_bot_buffer), i) + 1;
		size_t message_text_length = kp2018_spam_bot_string_length(messages[i & 0xF]);
		kp2018_spam_bot_buffer[message_length++] = ']';
		kp2018_spam_bot_buffer[message_length++] = ' ';
		memcpy(kp2018_spam_bot_buffer + message_length, messages[i & 0xF], message_text_length);

		error = ASK_TDMA_ERROR_PACKETS_LOST;
		while (error)
			error = client.send(ASK_RECEIVER_BROADCAST_ADDRESS, message_length + message_text_length, kp2018_spam_bot_buffer, &message_length);
	}
	
}