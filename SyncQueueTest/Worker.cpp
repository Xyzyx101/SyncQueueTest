#include "Worker.h"
#include <functional>
#include <iostream>
#include <chrono>
#include "Globals.h"

using namespace std::chrono;

high_resolution_clock::duration Worker::DoWork(Message message) {
	high_resolution_clock::time_point sleepUntil = high_resolution_clock::now() + SYNC_WORK_DURATION;
	while(high_resolution_clock::now() < sleepUntil) {
		// synchronous work on the cpu
	};
	std::this_thread::sleep_for(ASYNC_WORK_DURATION);
	std::chrono::high_resolution_clock::duration overhead = high_resolution_clock::now() - message.ReceiveTime;
	return overhead;
}

void Worker::Start(SyncQueue<Message>* in, SyncQueue<std::chrono::high_resolution_clock::duration>* out) {
	while(Running) {
		Message m;
		if(in->WaitGet(m)) {
			out->Put(DoWork(m));
		}
	}
}
