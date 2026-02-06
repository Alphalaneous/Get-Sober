#include <Geode/Geode.hpp>
#include <Geode/binding/LoadingLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCDirector.hpp>
#include "Config.hpp"
#include "FileExplorer.hpp"
#include "Console.hpp"
#include "Utils.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    if (sobriety::utils::isWine()) {
        FileExplorer::get()->setup();
        Console::get()->setup();
    }
    else {
        (void) Mod::get()->uninstall();
    }
}

class $modify(MenuLayer) {

    bool init() {
        if (!MenuLayer::init()) return false;

        queueInMainThread([] {
            if (!sobriety::utils::isWine()) {
                createQuickPopup("Windows User Detected!", "Sobriety only works on <cg>Linux</c> systems and will do nothing on <cb>Windows</c>.\nIt has been <cr>uninstalled</c>.", "OK", nullptr, nullptr);
            }
        });

        return true;
    }
};

class $modify(CCDirector) {

    void purgeDirector() {
        /*
            if this fails, the console wont exit, it shouldn't fail, but if it does, it isn't a big deal, as the user can close it themselves still
            imo a skill issue if writing to /tmp fails for any of these.
        */
        auto exitPath = Config::get()->getUniquePath() / "console.exit";
        auto exitRes = utils::file::writeString(exitPath, "");
        if (!exitRes) return log::error("Failed to create console exit file");
        CCDirector::purgeDirector();
    }
};

void geode_utils_game_exit_h(bool saveData) {
    auto exitPath = Config::get()->getUniquePath() / "console.exit";
    auto exitRes = utils::file::writeString(exitPath, "");
    if (!exitRes) return log::error("Failed to create console exit file");
    geode::utils::game::exit(saveData);
}

$on_mod(Loaded) {
    (void) Mod::get()->hook(
        reinterpret_cast<void*>(addresser::getNonVirtual(geode::modifier::Resolve<bool>::func(&geode::utils::game::exit))),
        &geode_utils_game_exit_h,
        "geode::utils::game::exit"
    );
}