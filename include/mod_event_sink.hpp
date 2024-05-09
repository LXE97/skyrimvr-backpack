#pragma once

template <typename T>
using EventCallback = void (*)(const T*);

template <typename T>
class EventSink : public RE::BSTEventSink<T>
{
public:
	static EventSink* GetSingleton()
	{
		static EventSink singleton;
		return &singleton;
	}

	void AddCallback(EventCallback<T> a_callback)
	{
		if (!a_callback) return;
		std::scoped_lock lock(callback_lock);
		callbacks.push_back(a_callback);
	}

	void RemoveCallback(EventCallback<T> a_callback)
	{
		if (!a_callback) return;
		std::scoped_lock lock(callback_lock);

		auto it = std::find(callbacks.begin(), callbacks.end(), a_callback);
		if (it != callbacks.end()) { callbacks.erase(it); }
	}

private:
	EventSink() = default;
	~EventSink() = default;
	EventSink(const EventSink&) = delete;
	EventSink(EventSink&&) = delete;
	EventSink& operator=(const EventSink&) = delete;
	EventSink& operator=(EventSink&&) = delete;

	RE::BSEventNotifyControl ProcessEvent(const T* a_event, RE::BSTEventSource<T>*)
	{
		std::scoped_lock lock(callback_lock);
		for (auto callback : callbacks) callback(a_event);
		return RE::BSEventNotifyControl::kContinue;
	}

	std::mutex                    callback_lock;
	std::vector<EventCallback<T>> callbacks;
};
