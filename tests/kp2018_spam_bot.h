#include "mbed.h"
#include "ask_tdma.h"

size_t kp2018_spam_bot_string_length(const char* string)
{
	const char* string_read = string;
	while (*string_read)
		++string_read;
	return (size_t)((uintptr_t)string_read - (uintptr_t)string);
}

void kp2018_spam_bot()
{
	const int f = 1000;
	const char* messages[16] = {
		"hello world", "hello world!", "hello world!!", "hello world!!!", "hello world!!!!",
		"test message 123456789", "TYPE 123 IN THE CHAT TO ENTER THE DOOR GIVEAWAY!", "123",
		"hello hello", "hello hell", "Santtu was here", "k", "xyzw", "test message", "",
		"The farad (symbol: F) is the SI derived unit of electrical capacitance, the ability of a body to store an electrical charge. It is named after the English physicist Michael Faraday. covfefe" };
	ask_tdma_client_t client;
	int error = client.init(D4, D2, f);
	uint8_t address;
	error = client.get_address(&address);
	for (int i = 0;; i = (i + 1) & 0xF)
	{
		size_t message_send;
		error = client.send(ASK_RECEIVER_BROADCAST_ADDRESS, kp2018_spam_bot_string_length(messages[i]), messages[i], &message_send);
	}
}