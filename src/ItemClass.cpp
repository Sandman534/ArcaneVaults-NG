#include "ItemClass.h"
#include "Settings.h"
#include "Utility.h"

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

CategoryMask ParseCategories(std::string_view a_value) {
    CategoryMask result = ToMask(ItemCategory::None);

    for (const auto part : std::views::split(a_value, ',')) {
        std::string name(part.begin(), part.end());

        const auto first = name.find_first_not_of(" \t");
        const auto last = name.find_last_not_of(" \t");

        if (first == std::string::npos)
            continue;

        name = name.substr(first, last - first + 1);

        if (const auto it = std::ranges::find_if(categoryNames, [&name](const auto& entry) {
                return entry.first == name;
            }); it != categoryNames.end()) {
            result |= it->second;
        } else {
            logger::warn("Unknown item category '{}'", name);
        }
    }

    return result;
}

std::string FormatCategories(CategoryMask a_categories) {
    std::string result;
    for (const auto& [name, category] : categoryNames) {
        if (!HasCategory(a_categories, category)) continue;
        if (!result.empty()) result += ", ";
        result += name;
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

[[nodiscard]] CategoryMask ExpandCategories(CategoryMask categories) noexcept {
    const auto weaponTypes =
        ItemCategory::OneHand |
        ItemCategory::TwoHand |
        ItemCategory::Archery |
        ItemCategory::Staff;

    if (HasAnyCategory(categories, weaponTypes))
        categories |= ItemCategory::Weapon;

    const auto armorTypes =
        ItemCategory::HeavyArmor |
        ItemCategory::LightArmor |
        ItemCategory::Clothing |
        ItemCategory::Jewelry;

    if (HasAnyCategory(categories, armorTypes))
        categories |= ItemCategory::Armor;

    const auto concoctionTypes =
        ItemCategory::Potion |
        ItemCategory::Poison;

    if (HasAnyCategory(categories, concoctionTypes))
        categories |= ItemCategory::Concoction;

    const auto smithingTypes =
        ItemCategory::Crafting |
        ItemCategory::Smelting |
        ItemCategory::Tanning |
        ItemCategory::Construction |
        ItemCategory::Gemstone;

    if (HasAnyCategory(categories, smithingTypes))
        categories |= ItemCategory::Smithing;

    const auto foodTypes =
        ItemCategory::RawFood |
        ItemCategory::CookedFood;

    if (HasAnyCategory(categories, foodTypes))
        categories |= ItemCategory::Food;

    return categories;
}

CategoryMask ClassifyItem(RE::TESBoundObject* a_item) noexcept {
    auto* utility = Utility::GetSingleton();
    if (!a_item) return ToMask(ItemCategory::None);

    // Overrides
    if (const auto override = Settings::GetSingleton()->FindOverride(a_item))
        return ExpandCategories(*override);

    CategoryMask categories = ToMask(ItemCategory::None);

    // Crafting
    if (utility->SmithingMap.contains(a_item->formID)) {
        categories |= ItemCategory::Smithing;
        categories |= ItemCategory::Crafting;
    }
    if (utility->SmeltingMap.contains(a_item->formID)) {
        categories |= ItemCategory::Smithing;
        categories |= ItemCategory::Smelting;
    }
    if (utility->TanningMap.contains(a_item->formID)) {
        categories |= ItemCategory::Smithing;
        categories |= ItemCategory::Tanning;
    }
    if (utility->ConstructionMap.contains(a_item->formID)) {
        categories |= ItemCategory::Smithing;
        categories |= ItemCategory::Construction;
    }
            
    // Ingredients
    if (auto* ingredient = a_item->As<RE::IngredientItem>()) {
        categories |= ItemCategory::Ingredient;

        if (ingredient->IsFood()) {
            categories |= ItemCategory::Food;

            if (utility->CookingMap.contains(a_item->formID))
                categories |= ItemCategory::Reagent;

            if (a_item->HasKeywordByEditorID("VendorItemFoodRaw") || 
                utility->RawFoodMap.contains(a_item->formID)
            )
                categories |= ItemCategory::RawFood;
        }

        return categories;
    }

    // Food, potions, and poisons
    if (auto* potion = a_item->As<RE::AlchemyItem>()) {
        if (potion->IsFood()) {
            categories |= ItemCategory::Food;

            if (a_item->HasKeywordByEditorID("VendorItemFoodRaw") || 
                utility->RawFoodMap.contains(a_item->formID)
            )
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

        const bool isJewelry =
            armor->HasKeywordByEditorID("VendorItemJewelry") ||
            armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kRing) ||
            armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kAmulet) ||
            armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kCirclet);

        if (isJewelry)
            categories |= ItemCategory::Jewelry;
        else if (armor->IsClothing())
            categories |= ItemCategory::Clothing;
        else if (armor->IsHeavyArmor())
            categories |= ItemCategory::HeavyArmor;
        else if (armor->IsLightArmor())
            categories |= ItemCategory::LightArmor;

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
            if (book->TeachesSpell()) 
                categories |= ItemCategory::SpellTome;
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
