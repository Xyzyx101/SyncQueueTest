// SyncQueueTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <future>
#include <locale>
#include "SyncQueue.h"
#include "Producer.h"
#include "Worker.h"
#include "WorkerPool.h"
#include "Resource.h"

using namespace std::chrono;
using namespace std::chrono_literals;

void AsyncSend(Producer* producer, const int64_t messageCount, high_resolution_clock::duration sendDuration) {
	producer->Produce(messageCount, sendDuration);
}

extern const std::chrono::high_resolution_clock::duration SYNC_WORK_DURATION{ 50us };
extern const std::chrono::high_resolution_clock::duration ASYNC_WORK_DURATION{ 50ms };
extern const std::chrono::high_resolution_clock::duration RECEIVE_DURATION{ 1s };
extern const int USERS{ 4 };
const int64_t MESSAGES_PER_SECOND = 100;
const int64_t MESSAGE_COUNT = duration_cast<seconds>(RECEIVE_DURATION).count() * MESSAGES_PER_SECOND;
const std::chrono::high_resolution_clock::duration TOTAL_WORK_DURATION{ (SYNC_WORK_DURATION + ASYNC_WORK_DURATION) * MESSAGE_COUNT };

void PrintResults(high_resolution_clock::duration total, high_resolution_clock::duration averageJob) {
	std::clog << "Total Duration\t\t:\t" << duration_cast<milliseconds>(total).count() << "ms" << std::endl;
	std::clog << "Average Job Overhead\t:\t" << duration_cast<microseconds>(averageJob).count() << "us" << std::endl;
	auto serverTime = total - RECEIVE_DURATION;
	std::clog << "Total Overhead\t\t:\t" << duration_cast<milliseconds>(serverTime).count() << "ms" << std::endl;
	std::clog << std::endl;
}

void PerformanceTests() {
	time_point<high_resolution_clock> start, end;
	high_resolution_clock::duration diff;
	std::clog << "Sending " << MESSAGE_COUNT << " jobs over " << duration_cast<milliseconds>(RECEIVE_DURATION).count() << "ms" << std::endl;
	std::clog << "Sync Work Duration\t:\t" << duration_cast<milliseconds>(SYNC_WORK_DURATION * MESSAGE_COUNT).count() << "ms" << std::endl;
	std::clog << "Async Work Duration\t:\t" << duration_cast<milliseconds>(ASYNC_WORK_DURATION * MESSAGE_COUNT).count() << "ms" << std::endl;
	std::clog << "Total Work Duration\t:\t" << duration_cast<milliseconds>(TOTAL_WORK_DURATION).count() << "ms" << std::endl;
	std::clog << std::endl;
	{
		std::clog << "Synchronous all at once" << std::endl;
		start = high_resolution_clock::now();
		SyncQueue<Message> q;
		Producer p{ q };
		p.Produce(MESSAGE_COUNT, RECEIVE_DURATION);
		std::vector<Message> messages;
		high_resolution_clock::duration average{ 0 };
		while(q.TryGetAll(messages)) {
			for(auto& m : messages) {
				average += Worker::DoWork(m);
			}
			messages.clear();
		}
		average /= MESSAGE_COUNT;
		end = high_resolution_clock::now();
		diff = end - start;
		PrintResults(diff, average);
	}
	{
		std::clog << "Synchronous one at a time" << std::endl;
		start = std::chrono::high_resolution_clock::now();
		SyncQueue<Message> q;
		Producer p{ q };
		std::vector<Message> messages;
		high_resolution_clock::duration average{ 0 };
		for(int i = 0; i < MESSAGE_COUNT; ++i) {
			p.Produce(1, RECEIVE_DURATION / MESSAGE_COUNT);
			while(q.TryGetAll(messages)) {
				for(auto& m : messages) {
					average += Worker::DoWork(m);;
				}
				messages.clear();
			}
		}
		average /= (int64_t)MESSAGE_COUNT;
		end = std::chrono::high_resolution_clock::now();
		diff = end - start;
		PrintResults(diff, average);
	}
	{
		std::clog << std::endl << "Naive Threaded Receive" << std::endl;
		start = std::chrono::high_resolution_clock::now();
		SyncQueue<Message> q;
		Producer p{ q };
		auto produceJob = std::async(std::launch::async, &AsyncSend, &p, MESSAGE_COUNT, RECEIVE_DURATION);
		int completed = 0;
		std::vector<Message> messages;
		high_resolution_clock::duration average{ 0 };
		while(completed < MESSAGE_COUNT) {
			if(q.TryGetAll(messages)) {
				for(auto& m : messages) {
					average += Worker::DoWork(m);
					++completed;
				}
				messages.clear();
			}
		}
		average /= (int64_t)MESSAGE_COUNT;
		produceJob.wait();
		end = std::chrono::high_resolution_clock::now();
		diff = end - start;
		PrintResults(diff, average);
	}
	{
		std::clog << std::endl << "CV Threaded Receive" << std::endl;
		start = std::chrono::high_resolution_clock::now();
		SyncQueue<Message> q;
		Producer p{ q };
		Worker w;
		auto produceJob = std::async(std::launch::async, &AsyncSend, &p, MESSAGE_COUNT, RECEIVE_DURATION);
		int completed = 0;
		std::vector<Message> messages;
		high_resolution_clock::duration average{ 0 };
		while(completed < MESSAGE_COUNT) {
			q.WaitGetAll(messages);
			for(auto& m : messages) {
				average += w.DoWork(m);
				++completed;
			}
			messages.clear();
		}
		average /= (int64_t)MESSAGE_COUNT;
		produceJob.wait();
		end = std::chrono::high_resolution_clock::now();
		diff = end - start;
		PrintResults(diff, average);
	}
	{
		std::clog << std::endl << "CV Threaded Receive, Naive Threaded Workers" << std::endl;
		start = std::chrono::high_resolution_clock::now();
		SyncQueue<Message> in;
		Producer p{ in };
		auto produceJob = std::async(std::launch::async, &AsyncSend, &p, MESSAGE_COUNT, RECEIVE_DURATION);
		int completed = 0;
		std::vector<Message> messages;
		high_resolution_clock::duration average{ 0 };
		std::vector<std::future<high_resolution_clock::duration>> allWork;
		while(completed < MESSAGE_COUNT) {
			in.WaitGetAll(messages);
			for(auto& m : messages) {
				// using one worker probably doesn't work in real code but DoWork has no state so meh...
				std::future<high_resolution_clock::duration> work = std::async(std::launch::async, &Worker::DoWork, m);
				allWork.emplace_back(std::move(work));
			}
			messages.clear();
			for(auto& w : allWork) {
				average += w.get();
				++completed;
			}
			allWork.clear();
		}
		average /= (int64_t)MESSAGE_COUNT;
		produceJob.wait();
		end = std::chrono::high_resolution_clock::now();
		diff = end - start;
		PrintResults(diff, average);
	}
	{
		std::clog << std::endl << "CV Threaded Receive, Worker Pool" << std::endl;
		start = std::chrono::high_resolution_clock::now();
		SyncQueue<Message> in;
		Producer p{ in };
		auto produceJob = std::async(std::launch::async, &AsyncSend, &p, MESSAGE_COUNT, RECEIVE_DURATION);
		int completed = 0;
		std::vector<Message> messages;
		high_resolution_clock::duration average{ 0 };
		std::unique_ptr<WorkerPool> workerPool = std::make_unique<WorkerPool>();
		while(completed < MESSAGE_COUNT) {
			in.WaitGetAllTimeout(messages);
			for(auto& m : messages) {
				workerPool->In.Put(m);
			}
			messages.clear();
			std::vector<high_resolution_clock::duration> allWork;
			workerPool->Out.TryGetAll(allWork);
			for(auto& w : allWork) {
				average += w;
				++completed;
			}
			allWork.clear();
		}
		average /= (int64_t)MESSAGE_COUNT;
		end = std::chrono::high_resolution_clock::now();
		produceJob.wait();
		workerPool.reset();
		diff = end - start;
		PrintResults(diff, average);
	}
}

void TaskManagerTest() {

}

int main() {
	std::locale::global(std::locale(""));
	std::clog.imbue(std::locale());

	/*auto dir = std::make_unique<Resource::Director>();
	auto token = dir->Acquire();
	std::clog << "Token : " << token << std::endl;*/

	PerformanceTests();
}
