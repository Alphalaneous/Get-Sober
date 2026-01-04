#include <Geode/Geode.hpp>
#include "FileExplorer.hpp"
#include "Console.hpp"
#include "Utils.hpp"

using namespace geode::prelude;

$execute {
    if (sobriety::utils::isWine()) {
        FileExplorer::get()->setup();
        Console::get()->setup();
    }
    else {
        (void) Mod::get()->uninstall();
    }
}