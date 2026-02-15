#include <Geode/Geode.hpp>
#include "Config.hpp"
#include "FileExplorer.hpp"
#include "Console.hpp"
#include "Utils.hpp"

using namespace geode::prelude;

void setupEvents() {
    GameEvent(GameEventType::Exiting).listen([] {
        auto exitPath = Config::get()->getUniquePath() / "console.exit";
        auto exitRes = utils::file::writeString(exitPath, "");
        if (!exitRes) return log::error("Failed to create console exit file");
    }).leak();

    GameEvent(GameEventType::Loaded).listen([] {
        if (!sobriety::utils::isWine()) {
            createQuickPopup("Windows User Detected!", "Sobriety only works on <cg>Linux</c> systems and will do nothing on <cb>Windows</c>.\nIt has been <cr>uninstalled</c>.", "OK", nullptr, nullptr);
        }
    }).leak();
}

$on_mod(Loaded) {
    if (sobriety::utils::isWine()) {
        FileExplorer::get()->setup();
        Console::get()->setup();
        setupEvents();
        return;
    }
    (void) Mod::get()->uninstall();
}
