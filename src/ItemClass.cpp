#include "ItemClass.h"
#include "Settings.h"
#include "Utility.h"

namespace
{
    const std::unordered_map<std::string_view, ItemCategory> categoryNames{
        { "Reagent", ItemCategory::Reagent },
        { "Catalyst", ItemCategory::Catalyst },
        { "Armor", ItemCategory::Armor },
        { "Clothing", ItemCategory::Clothing },
        { "Crafting", ItemCategory::Crafting },
        { "HeavyArmor", ItemCategory::HeavyArmor },
        { "LightArmor", ItemCategory::LightArmor },
        { "Shield", ItemCategory::Shield },
        { "Book", ItemCategory::Book },
        { "Construction", ItemCategory::Construction },
        { "Food", ItemCategory::Food },
        { "CookedFood", ItemCategory::CookedFood },
        { "RawFood", ItemCategory::RawFood },
        { "Gemstone", ItemCategory::Gemstone },
        { "Ingredient", ItemCategory::Ingredient },
        { "Concoction", ItemCategory::Concoction },
        { "Potion", ItemCategory::Potion },
        { "Poison", ItemCategory::Poison },
        { "Restorative", ItemCategory::Restorative },
        { "Scroll", ItemCategory::Scroll },
        { "Smelting", ItemCategory::Smelting },
        { "Smithing", ItemCategory::Smithing },
        { "Soulgem", ItemCategory::Soulgem },
        { "SoulgemEmpty", ItemCategory::EmptySoulgem },
        { "SoulgemFilled", ItemCategory::FilledSoulgem },
        { "SoulgemGrand", ItemCategory::GrandSoulgem },
        { "SpellTome", ItemCategory::SpellTome },
        { "Tanning", ItemCategory::Tanning },
        { "Treasure", ItemCategory::Treasure },
        { "Weapon", ItemCategory::Weapon },
        { "WeaponArchery", ItemCategory::Archery },
        { "WeaponOneHanded", ItemCategory::OneHand },
        { "WeaponTwoHanded", ItemCategory::TwoHand },
        { "WeaponStaff", ItemCategory::Staff }
    };
}

CategoryMask ToMask(ItemCategory a_category) noexcept {
    const auto bit = static_cast<std::uint8_t>(a_category);

    if (a_category == ItemCategory::None)
        return 0;

    return CategoryMask{ 1 } << bit;
}

CategoryMask operator|(ItemCategory a_lhs, ItemCategory a_rhs) noexcept {
    return ToMask(a_lhs) | ToMask(a_rhs);
}

CategoryMask operator|(CategoryMask a_lhs, ItemCategory a_rhs) noexcept {
    return a_lhs | ToMask(a_rhs);
}

CategoryMask operator|(ItemCategory a_lhs, CategoryMask a_rhs) noexcept {
    return ToMask(a_lhs) | a_rhs;
}

CategoryMask& operator|=(CategoryMask& a_lhs, ItemCategory a_rhs) noexcept {
    a_lhs |= ToMask(a_rhs);
    return a_lhs;
}

CategoryMask ParseCategories(std::string_view a_value)
{
    CategoryMask result = ToMask(ItemCategory::None);

    for (const auto part : std::views::split(a_value, ',')) {
        std::string name(part.begin(), part.end());

        const auto first = name.find_first_not_of(" \t");
        const auto last = name.find_last_not_of(" \t");

        if (first == std::string::npos)
            continue;

        name = name.substr(first, last - first + 1);

        if (const auto it = categoryNames.find(name);
            it != categoryNames.end()) {
            result |= it->second;
        } else {
            logger::warn("Unknown item category '{}'", name);
        }
    }

    return result;
}

bool HasCategory(CategoryMask a_categories, ItemCategory a_category) noexcept {
    return (a_categories & ToMask(a_category)) != 0;
}

bool HasAnyCategory(CategoryMask a_categories, CategoryMask a_wanted) noexcept {
    return (a_categories & a_wanted) != 0;
}

bool HasAllCategories(CategoryMask a_categories, CategoryMask a_wanted) noexcept {
    return (a_categories & a_wanted) == a_wanted;
}

CategoryMask ClassifyItem(RE::TESBoundObject* a_item) noexcept {
    auto* utility = Utility::GetSingleton();
    if (!a_item) return ToMask(ItemCategory::None);

    // Overrides
    if (const auto override = Settings::GetSingleton()->FindOverride(a_item))
        return *override;

    CategoryMask categories = ToMask(ItemCategory::None);

    // Crafting
    if (utility->SmithingMap.contains(a_item->formID))
        categories |= ItemCategory::Smithing;
    if (utility->SmeltingMap.contains(a_item->formID))
        categories |= ItemCategory::Smelting;
    if (utility->TanningMap.contains(a_item->formID))
        categories |= ItemCategory::Tanning;
    if (utility->ConstructionMap.contains(a_item->formID))
        categories |= ItemCategory::Construction;
            
    // Ingredients
    if (auto* ingredient = a_item->As<RE::IngredientItem>()) {
        categories |= ItemCategory::Ingredient;

        if (ingredient->IsFood()) {
            categories |= ItemCategory::Food;

            if (utility->CookingMap.contains(a_item->formID))
                categories |= ItemCategory::Reagent;

            if (a_item->HasKeywordByEditorID("VendorItemFoodRaw") || utility->RawFoodMap.contains(a_item->formID))
                categories |= ItemCategory::RawFood;
        }

        return categories;
    }

    // Food, potions, and poisons
    if (auto* potion = a_item->As<RE::AlchemyItem>()) {
        if (potion->IsFood()) {
            categories |= ItemCategory::Food;

            if (a_item->HasKeywordByEditorID("VendorItemFoodRaw") || utility->RawFoodMap.contains(a_item->formID))
                categories |= ItemCategory::RawFood;
            else 
                categories |= ItemCategory::CookedFood;

        } else {
            categories |= ItemCategory::Concoction;

            if (utility->FoundRestore(potion))
                categories |= ItemCategory::Restorative;

            if (potion->IsPoison())
                categories |= ItemCategory::Poison;
            else
                categories |= ItemCategory::Potion;
        }

        return categories;
    }

    // Armor and clothing
    if (auto* armor = a_item->As<RE::TESObjectARMO>()) {
        categories |= ItemCategory::Armor;

        if (armor->IsClothing())
            categories |= ItemCategory::Clothing;
        else if (armor->IsHeavyArmor())
            categories |= ItemCategory::HeavyArmor;
        else if (armor->IsLightArmor())
            categories |= ItemCategory::LightArmor;

        if (armor->IsShield())
            categories |= ItemCategory::Shield;

        return categories;
    }

    // Weapons
    if (auto* weapon = a_item->As<RE::TESObjectWEAP>()) {
        categories |= ItemCategory::Weapon;

        switch (weapon->GetWeaponType()) {
        case RE::WEAPON_TYPE::kOneHandSword:
        case RE::WEAPON_TYPE::kOneHandDagger:
        case RE::WEAPON_TYPE::kOneHandAxe:
        case RE::WEAPON_TYPE::kOneHandMace:
            categories |= ItemCategory::OneHand;
            break;

        case RE::WEAPON_TYPE::kTwoHandSword:
        case RE::WEAPON_TYPE::kTwoHandAxe:
            categories |= ItemCategory::TwoHand;
            break;

        case RE::WEAPON_TYPE::kBow:
        case RE::WEAPON_TYPE::kCrossbow:
            categories |= ItemCategory::Archery;
            break;

        case RE::WEAPON_TYPE::kStaff:
            categories |= ItemCategory::Staff;
            break;

        default:
            break;
        }

        return categories;
    }

    // Books, notes, scrolls, and spell tomes
    if (auto* book = a_item->As<RE::TESObjectBOOK>()) {
        if (book->IsNoteScroll())
            categories |= ItemCategory::Scroll;
        else {
            categories |= ItemCategory::Book;
            if (book->TeachesSpell()) categories |= ItemCategory::SpellTome;
        }

        return categories;
    }

    // Soul gems
    if (auto* soulGem = a_item->As<RE::TESSoulGem>()) {
        categories |= ItemCategory::Soulgem;

        if (soulGem->GetContainedSoul() == RE::SOUL_LEVEL::kNone)
            categories |= ItemCategory::EmptySoulgem;
        else
            categories |= ItemCategory::FilledSoulgem;

        if (soulGem->GetMaximumCapacity() == RE::SOUL_LEVEL::kGrand)
            categories |= ItemCategory::GrandSoulgem;

        return categories;
    }

    return categories;
}
