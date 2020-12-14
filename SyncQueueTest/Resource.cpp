#include "Resource.h"

using namespace Resource;

std::unique_ptr<Token> Director::Acquire(bool exclusive) {
	auto state = State.load(std::memory_order::memory_order_acquire);
	if( (state.Work & (1llu << 63llu)) > 0 ) { return nullptr; } // director can't hold anymore
	auto newState = CreateAcquireState(state, exclusive);
	while(!State.compare_exchange_weak(
		state, 
		newState, 
		std::memory_order::memory_order_release, 
		std::memory_order::memory_order_relaxed)) {
		newState = CreateAcquireState(state, exclusive);
	}
	auto token = new Token(newState.LastToken, shared_from_this());
	return std::unique_ptr<Token>(token);
}

inline WorkState_t Resource::Director::CreateAcquireState(WorkState_t in, bool exclusive) {
	++in.LastToken;
	in.Work = in.Work << 1llu | 1llu;
	in.Exclusive = in.Exclusive << 1llu | uint64_t(exclusive);
	return WorkState_t{ in.LastToken, in.Work, in.Exclusive };
}

bool Director::CanWork(TokenIdx index) {
	auto state = State.load(std::memory_order::memory_order_acquire);
	uint64_t shift = state.LastToken - index;
	int oldestIncomplete = 0;
	while((state.Work >>= 1llu) > 0llu) { oldestIncomplete++; }
	if(shift == oldestIncomplete) {
		return true; // the next incomplete token can always work
	}
	if(state.Exclusive & (1llu << shift)) {
		return false; // an exclusive work can never work with incomplete jobs ahead
	}
	int barrier = 0;
	while((state.Exclusive >>= 1llu) > 0llu) { barrier++; }
	// nothing can start with an exclusive job ahead
	return shift >= barrier;
}

void Director::Release(TokenIdx index) {
	auto state = State.load(std::memory_order::memory_order_acquire);
	auto newState = CreateReleaseState(state, index);
	while(!State.compare_exchange_weak(
		state,
		newState,
		std::memory_order::memory_order_release,
		std::memory_order::memory_order_relaxed)) {
		newState = CreateReleaseState(state, index);
	}
}

inline WorkState_t Director::CreateReleaseState(WorkState_t in, TokenIdx index) {
	uint64_t shift = in.LastToken - index;
	uint64_t off = ~(1ull << shift);
	in.Work &= off;
	in.Exclusive &= off;
	return WorkState_t{ in.LastToken, in.Work, in.Exclusive };
}

bool Token::CanWork() {
	if(auto dir = Director.lock()) {
		return dir->CanWork(Index);
	} else {
		throw Bad_Token_Error{};
	}
}

std::ostream& Resource::operator<<(std::ostream& out, const Token& token) {
	return out << "Token{" << token.Index << ", " << token.Director.lock() << '}';
}

std::ostream& Resource::operator<<(std::ostream& out, const TokenPtr& token) {
	if(token) {
		return out << *token;
	}
	return out << "Token{NULL}";
}

bool Resource::operator==(const Token& a, const Token& b) {
	return a.Index == b.Index && a.Director.lock() == b.Director.lock();
}

bool Resource::operator!=(const Token& a, const Token& b) {
	return !(a == b);
}

