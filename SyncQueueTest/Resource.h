#pragma once
#include <memory>
#include <type_traits>
#include <iostream>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <exception>

namespace Resource {
	struct Token;
	using TokenPtr = std::unique_ptr<Token>;
}

namespace Resource {
std::ostream& operator<<(std::ostream& out, const Resource::Token& token);
std::ostream& operator<<(std::ostream& out, const Resource::TokenPtr& token);
bool operator==(const Resource::Token& a, const Resource::Token& b);
bool operator!=(const Resource::Token& a, const Resource::Token& b);

	struct WorkState_t
	{
		/*WorkState_t() :
			LastToken(0),
			Work(0),
			Exclusive(0) {
		};*/
		uint64_t LastToken;
		uint64_t Work;
		uint64_t Exclusive;
	};
	using WorkState = std::atomic<WorkState_t>;
	using TokenIdx = uint64_t;
	class Director : public std::enable_shared_from_this<Director>
	{
	public:
		static std::shared_ptr<Director> Create() {
			/*std::clog << std::boolalpha << "WorkState is lock free? " << WorkState{}.is_lock_free() << std::endl;
			std::clog << std::boolalpha << "atomic_ullong is lock free? " << std::atomic_ullong{}.is_lock_free() << std::endl;*/
			return std::shared_ptr<Director>(new Director{});
		}
		friend struct Token;
		friend std::shared_ptr<Director>;
		friend std::weak_ptr<Director>;
		std::unique_ptr<Token> Acquire(bool exclusive = false);
		Director(const Token&) = delete;
		Director& operator= (const Token&) = delete;
	private:
		Director() :
			State(WorkState_t{ 0,0,0 }) {
		};
		bool CanWork(TokenIdx index);
		void Release(TokenIdx index);
		static inline WorkState_t CreateAcquireState(WorkState_t in, bool exclusive);
		static inline WorkState_t CreateReleaseState(WorkState_t in, TokenIdx index);
		WorkState State;
	};

	struct Token
	{
	private:
		friend std::unique_ptr<Token> Director::Acquire(bool);
		Token(TokenIdx index, std::weak_ptr<Director> userList) :
			Index(index),
			Director(userList) {
		};
	public:
		Token(const Token&) = delete;
		Token& operator= (const Token&) = delete;
		Token(Token&& other) noexcept :
			Index(other.Index),
			Director(std::move(other.Director)) {
		};
		Token& operator=(Token&& other) noexcept {
			Index = other.Index;
			Director.swap(other.Director);
			return *this;
		}
		~Token() noexcept {
			if(auto dir = Director.lock()) {
				dir->Release(Index);
			}
		}
		bool CanWork();
		friend std::ostream& (operator<<)(std::ostream& out, const Token& token);
		friend bool (operator==)(const Token& a, const Token& b);
		friend bool (operator!=)(const Token& a, const Token& b);
	private:
		std::weak_ptr<Director> Director;
		TokenIdx Index;
	};

	class Resource_Error : public std::exception {};
	class Bad_Token_Error : public Resource_Error
	{
		virtual const char* what() const noexcept {
			return "Cannot do work with a bad token!";
		}
	};
}







