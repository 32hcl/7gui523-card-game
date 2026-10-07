#pragma once
#include <string>
#include <vector>
#include "save_data.h"

class SaveManager {
public:
    SaveManager();
    ~SaveManager();

    bool save(const std::string& slotName, const SaveData& data);
    SaveData load(const std::string& slotName) const;
    bool remove(const std::string& slotName);
    std::vector<std::string> listSlots() const;

private:
    std::string m_saveDir;
};