#pragma once
#include <time.h>
#include <random>
#include "SyncQueue.h"
#include <chrono>
#include <thread>
#include "Message.h"

class Producer
{
public:
	Producer(SyncQueue<Message>& queue);
	void Produce(int64_t messageCount, std::chrono::high_resolution_clock::duration sendDuration);
private:
	Message RandomMessage();
	double Random() { return Dist(Gen); }
	SyncQueue<Message>& Queue;
	std::default_random_engine Gen;
	std::uniform_real_distribution<double> Dist;
};

