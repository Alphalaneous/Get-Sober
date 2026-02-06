#include "WaitingPopup.hpp"
#include "Geode/cocos/base_nodes/CCNode.h"
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include "Geode/ui/Layout.hpp"
#include "Geode/ui/LoadingSpinner.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

WaitingPopup* WaitingPopup::create() {
    auto ret = new WaitingPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool WaitingPopup::init() {
    if (!Popup::init(300, 100)) return false;
    m_bgSprite->setVisible(false);
    m_noElasticity = true;
    m_closeBtn->removeFromParent();
    setOpacity(180);
    
    auto container = CCNode::create();
    container->setAnchorPoint({0.5f, 0.5f});
    container->setContentSize({m_mainLayer->getContentWidth(), 50});
    container->setPosition(m_mainLayer->getContentSize()/2);

    auto layout = RowLayout::create();
    layout->setGap(15);
    layout->setAutoScale(false);
    layout->setAxisReverse(true);
    container->setLayout(layout);

    m_mainLayer->addChild(container);


    auto label = CCLabelBMFont::create("Please Pick a File", "bigFont.fnt");
    label->setScale(0.7f);
    container->addChild(label);

    auto spinner = LoadingSpinner::create(30);
    spinner->setAnchorPoint({1.f, 0.5f});

    container->addChild(spinner);

    container->updateLayout();

    return true;
}