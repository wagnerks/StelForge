#pragma once
#include <utility>


template<class ObjType>
class UniqueWeak;

struct RefCount {
	bool alive = 0;
	int weaks = 0;
};

template<class ObjType>
class Unique {
public:
	Unique(Unique&& other) noexcept
		: counter(other.counter),
		  obj(std::move(other.obj)) {
		other.counter = nullptr;
	}

	Unique& operator=(Unique&& other) noexcept {
		if (this == &other)
			return *this;

		counter = other.counter;
		other.counter = nullptr;
		obj = std::move(other.obj);
		return *this;
	}

	Unique(const Unique& other) = delete;
	Unique& operator=(const Unique& other) = delete;

	

	Unique(ObjType&& obj) : obj{ std::forward<ObjType>(obj) } {
		counter = new RefCount();
		counter->alive = true;
	}

	UniqueWeak<ObjType> getWeak() {
		return UniqueWeak<ObjType>(*this);
	}

	ObjType& get() {
		return obj;
	}

	ObjType& operator->() {
		return obj;
	}

	~Unique() {
		if (!counter) {
			return;
		}

		counter->alive = false;
		if (counter->weaks == 0) {
			delete counter;
		}
	}

private:
	RefCount* counter;
	ObjType obj;
};

template<class ObjType>
class UniqueWeak {
public:
	UniqueWeak(UniqueWeak&& other) = default;
	UniqueWeak& operator=(UniqueWeak&& other) = default;

	UniqueWeak(const UniqueWeak& other) : counter(other.counter), obj(other.obj) {
		counter->weaks++;
	}

	UniqueWeak& operator=(const UniqueWeak& other) {
		return this == &other ? *this : UniqueWeak(*this);
	}

	UniqueWeak(Unique<ObjType>& uniqe) {
		counter = uniqe.counter;
		counter->weaks++;
		obj = &uniqe.obj;
	}

	~UniqueWeak() {
		counter->weaks--;
		if (!counter->alive && counter->weaks == 0) {
			delete counter;
		}
	}

	ObjType* get() {
		if (counter->alive) {
			return obj;
		}

		return nullptr;
	}

private:
	RefCount* counter;
	ObjType* obj;
};