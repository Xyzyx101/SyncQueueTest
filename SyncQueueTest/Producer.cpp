#include "Producer.h"
#include <chrono>
using namespace std::chrono;

extern const int USERS;

Producer::Producer(SyncQueue<Message>& queue) : Queue(queue), Dist(0.0, 1.0) {
	Gen.seed((uint32_t)system_clock::now().time_since_epoch().count());
}

void Producer::Produce(int64_t messageCount, high_resolution_clock::duration sendDuration) {
	high_resolution_clock::duration interval = sendDuration / messageCount;
	
	/*high_resolution_clock::time_point start, end;
	start = high_resolution_clock::now();
	high_resolution_clock::duration sumSleepTime{ 0 };*/
	for(int count = 0; count < messageCount; ++count) {
		// intervals are randomly +/- 50%
		high_resolution_clock::duration sleepTime = high_resolution_clock::duration{ (int64_t)(interval.count() / 2 + Random() * interval.count()) };
		//sumSleepTime += sleepTime;
		high_resolution_clock::time_point sleepUntil = high_resolution_clock::now() + sleepTime;
		while(high_resolution_clock::now() < sleepUntil) {
			// this spin lock thrashes the cpu but this_thread::sleep_for only has a precision of 10ms which is useless
		};
		Queue.Put(RandomMessage());
	}
	/*end = high_resolution_clock::now();
	std::clog << "Produce Time : Target:" << duration_cast<milliseconds>(sendDuration).count() 
		<< "  Actual:" << duration_cast<milliseconds>(end - start).count() 
		<< "  Sum:" << duration_cast<milliseconds>(sumSleepTime).count()
		<< std::endl;*/
}

Message Producer::RandomMessage() {
	static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::string words;
	for(int i = 0; i < 8; ++i) {
		int randomIndex = (int)std::floor(Random() * strlen(alphanum));
		words += alphanum[randomIndex];
	}
	double number = Random();
	double otherNumber = Random() * 1000.;
	int user = (int)(Random() * (double)USERS);
	bool exclusive = (bool)(Random() < 0.5);
	return Message{
		words,
		Random(),
		Random() * 1000.,
		user,
		exclusive,
		std::chrono::high_resolution_clock::now()
	};
}
