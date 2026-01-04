#pragma once

#include <Geode/cocos/base_nodes/CCNode.h>
#include <chrono>
#include <functional>
#include <unordered_map>

struct ScheduledMethod {
    std::function<void()> method = nullptr;
    long long interval = 0;
    long long elapsedTime = 0;
};

class Scheduler : public cocos2d::CCNode {
public:
    static Scheduler* get();
    static Scheduler* create();

    template <class R, class P>
    void schedule(const std::string& id, std::function<void()>&& method, std::chrono::duration<R, P> interval) {
        m_scheduledMethods[id] = {
            std::move(method),
            std::chrono::duration_cast<std::chrono::milliseconds>(interval).count()
        };
    }

    void schedule(const std::string& id, std::function<void()>&& method) {
        m_scheduledMethods[id] = {
            std::move(method),
            0
        };
    }

    void unschedule(const std::string& id);

    void update(float dt);
private:
    std::unordered_map<std::string, ScheduledMethod> m_scheduledMethods;
};