#pragma once
#include "SyncQueue.h"
#include "Message.h"
#include <atomic>
#include "Message.h"

class TaskManager
{
	void AddUser(int userId);
	void RemoveUser(int userId);

private:
	void Tick();
	std::atomic_bool Running;
	SyncQueue<Message> In;
	SyncQueue<std::chrono::high_resolution_clock::duration> Out;
	std::vector<Message> Messages;
};



