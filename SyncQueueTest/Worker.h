#pragma once
#include "Message.h"
#include "SyncQueue.h"
#include <atomic>

class Worker
{
public:
	static std::chrono::high_resolution_clock::duration DoWork(Message message);
	Worker() : Running(true) {};

	void Start(SyncQueue<Message>* in, SyncQueue<std::chrono::high_resolution_clock::duration>* out);
	void Kill() { Running.store(false); }
	std::atomic_bool Running;
};

