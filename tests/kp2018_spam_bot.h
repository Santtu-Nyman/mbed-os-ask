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
	char message[0x100];
	DigitalOut led1(LED1, 0);
	const int f = 1000;
	const char* messages[16] = {
		"hello world", "hello world!", "hello world!!", "hello world!!!", "hello world!!!!",
		"test message 123456789", "TYPE 123 IN THE CHAT TO ENTER THE DOOR GIVEAWAY!", "123",
		"hello hello", "hello hell", "Santtu was here", "k", "xyzw", "test message", "",
		"The farad (symbol: F) is the SI derived unit of electrical capacitance, the ability of a body to store an electrical charge. It is named after the English physicist Michael Faraday. covfefe" };
	ask_tdma_client_t client;
	int error = client.init(D4, D2, f);
	uint8_t address;
	for (int i = 15;;/* i = (i + 1) & 0xF*/)
	{
		error = client.get_address(&address);
		size_t message_length = kp2018_spam_bot_string_length(messages[i]);
		message[0] = "0123456789ABCDEF"[(address >> 4) & 0xF];
		message[1] = "0123456789ABCDEF"[address & 0xF];
		message[2] = '>';
		message[3] = '>';
		memcpy(message + 4, messages[i], message_length);
		led1 = 1;
		error = client.send(ASK_RECEIVER_BROADCAST_ADDRESS, 4 + message_length, message, &message_length);
		led1 = 0;
		//wait_us(1000000);
	}
}