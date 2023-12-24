#pragma once
// lets you add/remove callbacks to event sources at runtime... not sure why I thought I needed this 

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

    void AddCallback(EventCallback<T> callback)
    {
        if (!callback) return;
        std::scoped_lock lock(callbackLock);
        callbacks.push_back(callback);
    }

    void RemoveCallback(EventCallback<T> callback)
    {
        if (!callback) return;
        std::scoped_lock lock(callbackLock);
        auto it = std::find(callbacks.begin(), callbacks.end(), callback);
        if (it != callbacks.end()) {
            callbacks.erase(it);
        }
    }

private:
    EventSink() = default;
    EventSink(const EventSink&) = delete;
    EventSink& operator=(const EventSink&) = delete;

    std::mutex callbackLock;
    std::vector<EventCallback<T>> callbacks;

    RE::BSEventNotifyControl ProcessEvent(const T* event, RE::BSTEventSource<T>*)
    {
        std::scoped_lock lock(callbackLock);
        for (auto callback : callbacks)
            callback(event);
        return RE::BSEventNotifyControl::kContinue;
    }
};
