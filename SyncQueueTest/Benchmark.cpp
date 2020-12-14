// Alternatively, can add libraries using linker options.
#ifdef _WIN32
#pragma comment ( lib, "Shlwapi.lib" )
#ifdef _DEBUG
#pragma comment ( lib, "benchmarkd.lib" )
#else
#pragma comment ( lib, "benchmark.lib" )
#endif
#endif

#include "benchmark/benchmark.h"
#include "Resource.h"
#include <future>

using namespace Resource;

static void BM_Acquire(benchmark::State& state) {
	auto dir = Resource::Director::Create();
	for(auto _ : state) {
		auto exclusiveToken = dir->Acquire(false);
	}
}
BENCHMARK(BM_Acquire);

static void BM_AcquireExclusive(benchmark::State& state) {
	auto dir = Resource::Director::Create();
	for(auto _ : state) {
		auto exclusiveToken = dir->Acquire(true);
	}
}
BENCHMARK(BM_Acquire);

class Worker
{
public:
	void operator()(std::shared_ptr<Director> dir, int work) {
		const int exclusiveInterval = 5;
		int jobsComplete = 0;
		while(jobsComplete < work) {
			bool isExclusive = jobsComplete % exclusiveInterval == 1;
			if(auto job = dir->Acquire(isExclusive)) {
				while(!job->CanWork()) {
					//std::this_thread::yield();
				}
				job.reset();
				++jobsComplete;
			}
		}
	}
};

static void BM_Multiworker(benchmark::State& state) {
	auto dir = Resource::Director::Create();
	const int work = state.range(0);
	const int workerCount = state.range(1);
	for(auto _ : state) {
		std::vector<std::future<void>> workers;
		for(int i = 0; i < workerCount; ++i) {
			workers.emplace_back(std::async(std::launch::async, Worker(), dir, work / workerCount));
		}
		for(int i = 0; i < workerCount; ++i) {
			workers[i].get();
		}
	}
}
BENCHMARK(BM_Multiworker)
	->RangeMultiplier(2)
	->Ranges({ {1 << 4, 1 << 8}, {1 << 0, 1 << 3} })
	->MeasureProcessCPUTime()
	->UseRealTime();;

BENCHMARK_MAIN();