#pragma once

#include <string>
#include <iostream>
#include <chrono>

struct Message
{
	std::string Words;
	double Number;
	double OtherNumber;
	int User;
	bool Exclusive;
	std::chrono::time_point<std::chrono::steady_clock> ReceiveTime;
	friend std::ostream& operator<<(std::ostream& os, const Message& message) {
		os << message.Words << " : " << message.Number << " : " << message.OtherNumber;
		return os;
	}
};

