#include "save_manager.h"

SaveManager::SaveManager() : m_saveDir("saves/") {}
SaveManager::~SaveManager() = default;

bool SaveManager::save(const std::string& slotName, const SaveData& data) {
    (void)slotName;
    (void)data;
    return false;
}

SaveData SaveManager::load(const std::string& slotName) const {
    (void)slotName;
    return SaveData{};
}

bool SaveManager::remove(const std::string& slotName) {
    (void)slotName;
    return false;
}

std::vector<std::string> SaveManager::listSlots() const {
    return {};
}