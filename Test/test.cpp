#include "pch.h"
#include "../SyncQueueTest/Resource.h"
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <future>

using namespace Resource;
using namespace std::literals::chrono_literals;
TEST(Resource, Basic_Operation) {
	auto dir = Director::Create();
	ASSERT_TRUE(dir) << "Create should return a director";
	auto token = dir->Acquire();
	ASSERT_TRUE(token) << "Director->Acquire() should create token";
	auto token2 = dir->Acquire();
	std::clog << token2 << std::endl;
	EXPECT_NE(*token, *token2) << "Two tokens should be different";
	token.reset();
	token2.reset();
	EXPECT_EQ(token, token2) << "Tokens are completed on destruction";
}

TEST(Resource, Can_Work) {
	auto dir = Director::Create();
	auto token = dir->Acquire();
	EXPECT_TRUE(token->CanWork()) << "New token can do work";
}

TEST(Resource, Exclusive_Can_Work) {
	auto dir = Director::Create();
	auto token = dir->Acquire(true);
	EXPECT_TRUE(token->CanWork()) << "New exclusive token can do work";
}

TEST(Resource, Multiple_Tokens_Can_Work) {
	auto dir = Director::Create();
	auto token1 = dir->Acquire();
	EXPECT_TRUE(token1->CanWork()) << "token1 can do work";
	auto token2 = dir->Acquire();
	EXPECT_TRUE(token2->CanWork()) << "token2 can do work";
	auto token3 = dir->Acquire();
	EXPECT_TRUE(token3->CanWork()) << "token3 can do work";
}

TEST(Resource, Multiple_Exclusive_Tokens_Can_Not_Work) {
	auto dir = Director::Create();
	auto token1 = dir->Acquire(true);
	EXPECT_TRUE(token1->CanWork()) << "token1 can do work";
	auto token2 = dir->Acquire(true);
	EXPECT_FALSE(token2->CanWork()) << "token2 can not do work";
	auto token3 = dir->Acquire(true);
	EXPECT_FALSE(token3->CanWork()) << "token3 can not do work";
}

TEST(Resource, Exclusive_Cannot_Work_With_Incomplete_Work_Ahead) {
	auto dir = Director::Create();
	auto token = dir->Acquire();
	EXPECT_TRUE(token->CanWork()) << "New token can do work";
	auto token2 = dir->Acquire();
	EXPECT_TRUE(token2->CanWork()) << "New token2 can do work";
	auto exclusiveToken = dir->Acquire(true);
	EXPECT_FALSE(exclusiveToken->CanWork()) << "Exclusive cannot start until oustanding work is done";
	token.reset();
	token2.reset();
	EXPECT_TRUE(exclusiveToken->CanWork()) << "Exclusive can now work";
}

TEST(Resource, Non_Exclusive_Cannot_Work_With_Incomplete_Exclusive_Ahead) {
	auto dir = Director::Create();
	auto exclusiveToken = dir->Acquire(true);
	EXPECT_TRUE(exclusiveToken->CanWork()) << "Exclusive cannot start until oustanding work is done";
	auto token1 = dir->Acquire();
	EXPECT_FALSE(token1->CanWork()) << "Token1 can not work";
	auto token2 = dir->Acquire();
	EXPECT_FALSE(token2->CanWork()) << "Token2 can not work";
	exclusiveToken.reset();
	EXPECT_TRUE(token1->CanWork() && token2->CanWork()) << "Both can work after exclusive is complete";
}

/* In real code you would do the work and wait for it to complete before destroying the token */
std::vector<TokenPtr> DoWork(std::vector<TokenPtr>& work) {
	auto firstCannot = std::find_if(std::begin(work), std::end(work), [](TokenPtr& t) { return !t->CanWork(); });
	std::vector<TokenPtr> remaining;
	remaining.reserve(work.capacity());
	for(; firstCannot != std::end(work); ++firstCannot) {
		remaining.emplace_back(std::move(*firstCannot));
	}
	return remaining;
}

TEST(Resource, Exclusive_Blocks_Non_Exclusive) {
	auto dir = Director::Create();
	std::vector<TokenPtr> work;
	work.emplace_back(dir->Acquire());
	work.emplace_back(dir->Acquire());
	work.emplace_back(dir->Acquire(true));
	work.emplace_back(dir->Acquire());
	work.emplace_back(dir->Acquire());
	work.emplace_back(dir->Acquire(true));
	work.emplace_back(dir->Acquire());
	work.emplace_back(dir->Acquire());
	EXPECT_EQ(work.size(), 8) << "Add 8 jobs";
	work = DoWork(work);
	EXPECT_EQ(work.size(), 6) << "Up to exclusive is complete";
	work = DoWork(work);
	EXPECT_EQ(work.size(), 5) << "First exclusive is complete";
	work = DoWork(work);
	EXPECT_EQ(work.size(), 3) << "Up to second exclusive is complete";
	work = DoWork(work);
	EXPECT_EQ(work.size(), 2) << "Second exclusive is complete";
	work = DoWork(work);
	EXPECT_EQ(work.size(), 0) << "Done";
}

TEST(Resource, Non_Exclusive_Complete_Order_Does_Not_Matter) {
	auto dir = Director::Create();
	std::vector<TokenPtr> work;
	auto token1 = dir->Acquire();
	auto token2 = dir->Acquire();
	auto token3 = dir->Acquire();
	auto exclusiveToken = dir->Acquire(true);
	EXPECT_FALSE(exclusiveToken->CanWork()) << "Exclusive blocked";
	token2.reset();
	EXPECT_FALSE(exclusiveToken->CanWork()) << "Exclusive still blocked";
	token1.reset();
	EXPECT_FALSE(exclusiveToken->CanWork()) << "Exclusive still blocked...again";
	token3.reset();
	EXPECT_TRUE(exclusiveToken->CanWork()) << "Exclusive still blocked...again";
}

TEST(Resource, Token_Will_Be_NULL_When_Director_Is_Full) {
	auto dir = Director::Create();
	std::vector<TokenPtr> work;
	for(int i = 0; i < 64; ++i) {
		work.emplace_back(dir->Acquire());
		EXPECT_TRUE(work.back());
	}
	EXPECT_FALSE(dir->Acquire()) << "Cannot aqcuire more than 64 tokens";
	work.erase(std::begin(work));
	EXPECT_TRUE(dir->Acquire()) << "Can aqcuire more after complete";
}

struct Worker
{
	void operator()(int workerIdx, Director* dir, std::mutex* mut, std::vector<std::string>* results) {
		const int jobCount = 64;
		const int exclusiveInterval = 5;
		int jobsComplete = 0;
		while(jobsComplete < jobCount) {
			bool isExclusive = jobsComplete % exclusiveInterval == 1;
			if(auto job = dir->Acquire(isExclusive)) {
				while(!job->CanWork()) {
					// stupid spin...
				}
				std::stringstream ss;
				if(isExclusive) { ss << "Ex: "; }
				ss << "Start => W:" << workerIdx << ' ' << job;
				{
					std::scoped_lock lock(*mut);
					results->push_back(ss.str());
				}
				std::this_thread::sleep_for(0ms);
				ss.str(std::string{});
				if(isExclusive) { ss << "Ex: "; }
				ss << "Done  => W:" << workerIdx << ' ' << job;
				{
					std::scoped_lock lock(*mut);
					results->push_back(ss.str());
				}
				++jobsComplete;
			} 
		}
	}
};

int getTokenId(const std::string& str) {
	size_t tokenLoc = str.find("Token{");
	std::string tokenStr = str.substr(tokenLoc + 6);
	return std::stoi(tokenStr);
}

//TEST(Resource, Exclusive_Even_With_Multiple_Work_Threads) {
//	std::mutex mtx;
//	std::vector<std::string> results;
//	auto dir = Director::Create();
//	const int workerCount = 8;
//	std::vector<std::future<void>> workers;
//	for(int i = 0; i < workerCount; ++i) {
//		workers.emplace_back(std::async(std::launch::async, Worker(), i, dir.get(), &mtx, &results));
//	}
//	//std::this_thread::sleep_for(1s);
//	bool done = false;
//	auto timeout = std::async(std::launch::async, [&done]() {
//		std::this_thread::sleep_for(1s);
//		EXPECT_TRUE(done) << "Timed Out!  Possible deadlock!";
//		});
//	for(auto& w : workers) {
//		w.get();
//	}
//	done = true;
//	timeout.get();
//	for(auto startIt = results.begin(); startIt < results.end(); ++startIt) {
//		std::clog << *startIt << std::endl;
//		if(startIt->find("Ex: Start") != std::string::npos) {
//			auto endIt = startIt + 1;
//			EXPECT_TRUE(endIt->find("Ex: Done") != std::string::npos) << "Nothing between start and end of exclusive jobs";
//			EXPECT_EQ(getTokenId(*startIt), getTokenId(*endIt)) << "Start and end should have the same token";
//		}
//	}
//}