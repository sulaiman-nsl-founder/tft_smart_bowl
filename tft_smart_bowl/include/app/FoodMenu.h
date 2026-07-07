#ifndef APP_FOOD_MENU_H
#define APP_FOOD_MENU_H

#include <stdint.h>
#include <stddef.h>

namespace App {

/**
 * @brief Represents a single food profile.
 */
struct FoodProfile {
    uint32_t id;
    char name[32];
    float caloriesPerGram;
};

/**
 * @brief Manages the list of available foods and the currently selected food.
 */
class FoodMenu {
public:
    static FoodMenu& getInstance();

    void begin();

    /**
     * @brief Get the currently selected food profile.
     * @param outProfile Pointer to store the profile.
     * @return true if a valid food is selected, false if no menu/selection.
     */
    bool getSelectedFood(FoodProfile* outProfile) const;

    /**
     * @brief Set the currently selected food by ID.
     */
    bool selectFood(uint32_t id);

private:
    FoodMenu() = default;
    ~FoodMenu() = default;
    FoodMenu(const FoodMenu&) = delete;
    FoodMenu& operator=(const FoodMenu&) = delete;

    static constexpr size_t MAX_FOODS = 5;
    FoodProfile _foods[MAX_FOODS];
    size_t _foodCount = 0;
    
    int32_t _selectedIndex = -1; // -1 means no selection
};

} // namespace App

#endif // APP_FOOD_MENU_H
