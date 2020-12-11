#pragma once
#include "SyncQueue.h"
#include "Message.h"
#include "Worker.h"
#include <chrono>
#include <future>

class WorkerPool
{
public:
	WorkerPool() {
		const int workerCount = std::thread::hardware_concurrency() * 8;
		for(int i = 0; i < workerCount; ++i) {
			Workers.emplace_back(std::make_unique<Worker>());
			std::future<void> result = std::async(std::launch::async, &Worker::Start, Workers.back().get(), &In, &Out);
			Results.emplace_back(std::move(result));
		}
	}
	~WorkerPool() {
		for(auto& wPtr : Workers) {
			wPtr->Kill();
		}
		for(auto& r : Results) {
			r.get();
		}
	}
	SyncQueue<Message> In;
	SyncQueue<std::chrono::high_resolution_clock::duration> Out;
private:
	std::vector<std::unique_ptr<Worker>> Workers;
	std::vector<std::future<void>> Results;
};

