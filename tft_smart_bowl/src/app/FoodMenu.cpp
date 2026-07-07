#include "app/FoodMenu.h"
#include <string.h>

namespace App {

FoodMenu& FoodMenu::getInstance() {
    static FoodMenu instance;
    return instance;
}

void FoodMenu::begin() {
    // For now, hardcode a couple of food profiles for testing
    // Later, these would be downloaded from the cloud or configured via mobile app
    
    _foods[0].id = 1;
    strncpy(_foods[0].name, "Dry Kibble", sizeof(_foods[0].name) - 1);
    _foods[0].caloriesPerGram = 3.5f;
    
    _foods[1].id = 2;
    strncpy(_foods[1].name, "Wet Food", sizeof(_foods[1].name) - 1);
    _foods[1].caloriesPerGram = 1.2f;
    
    _foodCount = 2;
    _selectedIndex = 0; // Default to Dry Kibble
}

bool FoodMenu::getSelectedFood(FoodProfile* outProfile) const {
    if (_selectedIndex >= 0 && _selectedIndex < (int32_t)_foodCount && outProfile != nullptr) {
        *outProfile = _foods[_selectedIndex];
        return true;
    }
    return false;
}

bool FoodMenu::selectFood(uint32_t id) {
    for (size_t i = 0; i < _foodCount; i++) {
        if (_foods[i].id == id) {
            _selectedIndex = (int32_t)i;
            return true;
        }
    }
    return false;
}

} // namespace App
