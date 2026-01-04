#include <Geode/Geode.hpp>
#include "Scheduler.hpp"

using namespace geode::prelude;

Scheduler* Scheduler::get() {
    static Scheduler* instance = Scheduler::create();
    return instance;
}

Scheduler* Scheduler::create() {
    auto ret = new Scheduler();
    if (ret->init()) {
        ret->autorelease();
        ret->onEnter();
        return ret;
    }
    delete ret;
    return nullptr;
}

void Scheduler::unschedule(const std::string& id) {
    m_scheduledMethods.erase(id);
}

void Scheduler::update(float dt) {
    for (auto& [k, v] : m_scheduledMethods) {
        v.elapsedTime += dt * 1000;
        if (v.elapsedTime >= v.interval) {
            if (v.method) v.method();
            v.elapsedTime -= v.interval;
        }
    }
}

$execute {
    CCScheduler::get()->scheduleUpdateForTarget(Scheduler::get(), INT_MIN, false);
}