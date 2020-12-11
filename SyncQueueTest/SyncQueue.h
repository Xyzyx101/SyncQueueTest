#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
using namespace std::chrono_literals;

template<typename Message>
class SyncQueue
{
public:
	void Put(const Message& message) {
		std::lock_guard<std::mutex> lock{ Mtx };
		Messages.push_back(message);
		Cond.notify_one();
	}
	void Put(Message&& message) {
		std::lock_guard<std::mutex> lock{ Mtx };
		Messages.emplace_back(message);
		Cond.notify_one();
	}
	bool TryGetAll(std::vector<Message>& outMessages) {
		std::lock_guard<std::mutex> lock{ Mtx };
		if(Messages.empty()) { return false; }
		MoveMessages(outMessages);
		return true;
	}
	void WaitGetAll(std::vector<Message>& outMessages) {
		std::unique_lock<std::mutex> lock{ Mtx };
		Cond.wait(lock, [this] { return !Messages.empty(); }); // the predicate prevents spurious wakeup
		MoveMessages(outMessages);
	}
	void WaitGetAllTimeout(std::vector<Message>& outMessages) {
		std::unique_lock<std::mutex> lock{ Mtx };
		Cond.wait_for(lock, 1ms, [this] { return !Messages.empty(); }); // the predicate prevents spurious wakeup
		MoveMessages(outMessages);
	}
	bool WaitGet(Message& out) {
		std::unique_lock<std::mutex> lock{ Mtx };
		if(!Messages.empty()) {
			std::swap(out, Messages.front());
			Messages.erase(std::begin(Messages));
			return true;
		}
		Cond.wait_for(lock, 100ms, [this] { return !Messages.empty(); });
		if(!Messages.empty()) {
			std::swap(out, Messages.front());
			Messages.erase(std::begin(Messages));
			return true;
		}
		return false;
	}
private:
	void MoveMessages(std::vector<Message>& to) {
		to.reserve(Messages.size());
		for(auto& m : Messages) {
			to.emplace_back(std::move(m));
		}
		Messages.clear();
	}
	std::vector<Message> Messages;
	std::condition_variable Cond;
	std::mutex Mtx;
};

