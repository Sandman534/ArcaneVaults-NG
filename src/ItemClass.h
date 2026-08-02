#pragma once

#include <cstdint>

enum class Container : std::uint8_t
{
    None,
    Master,
    Sort,
    Alchemy,
    Armor,
    Clothing,
    HeavyArmor,
    LightArmor,
    Shield,
    Book,
    Food,
    CookedFood,
    RawFood,
    Reagent,
    Follower,
    Gemstone,
    Concoction,
    Potion,
    Poison,
    Restorative,
    Scroll,
    Smithing,
    Construction,
    Crafting,
    Smelting,
    Tanning,
    Soulgem,
    Treasure,
    Stolen,
    Weapon,
    Archery,
    OneHand,
    TwoHand,
    Staff
};

enum class ItemCategory : std::uint8_t
{
    Reagent,
    Catalyst,
    Armor,
    Clothing,
    HeavyArmor,
    LightArmor,
    Shield,
    Book,
    Construction,
    Crafting,
    Food,
    CookedFood,
    RawFood,
    Gemstone,
    Ingredient,
    Concoction,
    Potion,
    Poison,
    Restorative,
    Scroll,
    Smelting,
    Smithing,
    Soulgem,
    EmptySoulgem,
    FilledSoulgem,
    GrandSoulgem,
    SpellTome,
    Tanning,
    Treasure,
    Weapon,
    Archery,
    OneHand,
    TwoHand,
    Staff,
    None
};

enum class ContainerGroup
{
    Special,
    Vault,
    Archive
};

using CategoryMask = std::uint64_t;

[[nodiscard]] CategoryMask ToMask(ItemCategory a_category) noexcept;

[[nodiscard]] CategoryMask operator|(ItemCategory a_lhs, ItemCategory a_rhs) noexcept;
[[nodiscard]] CategoryMask operator|(CategoryMask a_lhs, ItemCategory a_rhs) noexcept;
[[nodiscard]] CategoryMask operator|(ItemCategory a_lhs, CategoryMask a_rhs) noexcept;
CategoryMask& operator|=(CategoryMask& a_lhs, ItemCategory a_rhs) noexcept;

[[nodiscard]] bool HasCategory(CategoryMask a_categories, ItemCategory a_category) noexcept;
[[nodiscard]] bool HasAnyCategory(CategoryMask a_categories, CategoryMask a_wanted) noexcept;
[[nodiscard]] bool HasAllCategories(CategoryMask a_categories, CategoryMask a_wanted) noexcept;

[[nodiscard]] CategoryMask ParseCategories(std::string_view a_value);
[[nodiscard]] CategoryMask ClassifyItem(RE::TESBoundObject* a_item) noexcept;
