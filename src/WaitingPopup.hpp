#pragma once

#include <Geode/ui/Popup.hpp>

class WaitingPopup : public geode::Popup {
public:
    static WaitingPopup* create();
protected:
    bool init();
};