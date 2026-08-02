#pragma once
#include "ItemClass.h"
#include "Utility.h"

using MenuAction = std::function<void()>;

struct MenuOption
{
    std::string text;
    MenuAction action;
};

class MenuCallback final : public RE::IMessageBoxCallback {
public:
    explicit MenuCallback(std::vector<MenuAction> a_actions) : actions(std::move(a_actions)){}

    void Run(Message a_message) override {
        const auto index = static_cast<std::size_t>(a_message);
        if (index >= actions.size() || !actions[index]) return;

        // Copy it because this callback may be destroyed after Run returns.
        auto action = actions[index];

        SKSE::GetTaskInterface()->AddTask([action = std::move(action)]() mutable {
            action();
        });
    }

private:
    std::vector<MenuAction> actions;
};

class Stores {
public:
    static Stores* GetSingleton() {
        static Stores singleton;
        return &singleton;
    }

    // Storage Menus
    void ShowDynamicMessageBox(std::string_view a_message, std::vector<MenuOption> a_options, std::int32_t a_cancelOption = -1);

    // Storage Functions
    void PlayerToStore(ItemCategory a_category, Container a_chest, int i_count = 0);
    void StoreToPlayer(ItemCategory a_category, Container a_chest, int i_count = 0);
    void StoreToStore(ItemCategory a_category, Container a_source, Container a_destination);
    void AllToStore(Container a_source, Container a_destination);
    void AllToPlayer(Container a_source);
    void OpenStorageMenu(Container a_chest, RE::TESObjectREFR* a_source, std::function<void(RE::TESObjectREFR*)> a_onClose);

    // Store Animations
    using AnimationCallback = std::function<void(RE::TESObjectREFR*)>;

    void OpenStoreObject(RE::TESObjectREFR* a_source, AnimationCallback a_onComplete = {});
    void CloseStoreObject(RE::TESObjectREFR* a_source, AnimationCallback a_onComplete = {});

    // Utility Functions
    void SortItems();
    void OffloadItems();
    void VaultAssignment(RE::TESObjectREFRPtr a_source);

    // Container Functions
    void VaultMenu(RE::TESObjectREFR* a_source);
    void VaultDetail1Menu(RE::TESObjectREFR* a_source);
    void VaultDetail2Menu(RE::TESObjectREFR* a_source);
    void SortingMenu(RE::TESObjectREFR* a_source);
    void AlchemyMenu(RE::TESObjectREFR* a_source);
    void SoulgemMenu(RE::TESObjectREFR* a_source);
    void FoodMenu(RE::TESObjectREFR* a_source);
    void FoodCookedMenu(RE::TESObjectREFR* a_source);
    void FoodRawMenu(RE::TESObjectREFR* a_source);
    void FoodSaltMenu(RE::TESObjectREFR* a_source);
    void BookMenu(RE::TESObjectREFR* a_source);
    void ScrollMenu(RE::TESObjectREFR* a_source);
    void ArmorMenu(RE::TESObjectREFR* a_source);
    void ArmorLightMenu(RE::TESObjectREFR* a_source);
    void ArmorHeavyMenu(RE::TESObjectREFR* a_source);
    void ArmorShieldMenu(RE::TESObjectREFR* a_source);
    void ArmorClothingMenu(RE::TESObjectREFR* a_source);
    void WeaponMenu(RE::TESObjectREFR* a_source);
    void WeaponArcheryMenu(RE::TESObjectREFR* a_source);
    void WeaponOneHandMenu(RE::TESObjectREFR* a_source);
    void WeaponTwoHandMenu(RE::TESObjectREFR* a_source);
    void WeaponStaffMenu(RE::TESObjectREFR* a_source);
    void GemstoneMenu(RE::TESObjectREFR* a_source);
    void TreasureMenu(RE::TESObjectREFR* a_source);
    void StolenMenu(RE::TESObjectREFR* a_source);
    void FollowerMenu(RE::TESObjectREFR* a_source);
    void PotionMenu(RE::TESObjectREFR* a_source);
    void PotionPositiveMenu(RE::TESObjectREFR* a_source);
    void PotionNegativeMenu(RE::TESObjectREFR* a_source);
    void PotionRestoreMenu(RE::TESObjectREFR* a_source);
    void SmithMenu(RE::TESObjectREFR* a_source);
    void SmithCraftMenu(RE::TESObjectREFR* a_source);
    void SmithSmeltMenu(RE::TESObjectREFR* a_source);
    void SmithTanMenu(RE::TESObjectREFR* a_source);
    void SmithConstructMenu(RE::TESObjectREFR* a_source);

private:
    void ShowVaultMenu(RE::TESObjectREFR* a_source);
    void ShowVaultDetail1Menu(RE::TESObjectREFR* a_source);
    void ShowVaultDetail2Menu(RE::TESObjectREFR* a_source);
    void ShowSortingMenu(RE::TESObjectREFR* a_source);
    void ShowAlchemyMenu(RE::TESObjectREFR* a_source);
    void ShowSoulgemMenu(RE::TESObjectREFR* a_source);
    void ShowFoodMenu(RE::TESObjectREFR* a_source);
    void ShowFoodCookedMenu(RE::TESObjectREFR* a_source);
    void ShowFoodRawMenu(RE::TESObjectREFR* a_source);
    void ShowFoodSaltMenu(RE::TESObjectREFR* a_source);
    void ShowBookMenu(RE::TESObjectREFR* a_source);
    void ShowScrollMenu(RE::TESObjectREFR* a_source);
    void ShowArmorMenu(RE::TESObjectREFR* a_source);
    void ShowArmorLightMenu(RE::TESObjectREFR* a_source);
    void ShowArmorHeavyMenu(RE::TESObjectREFR* a_source);
    void ShowArmorShieldMenu(RE::TESObjectREFR* a_source);
    void ShowArmorClothingMenu(RE::TESObjectREFR* a_source);
    void ShowWeaponMenu(RE::TESObjectREFR* a_source);
    void ShowWeaponArcheryMenu(RE::TESObjectREFR* a_source);
    void ShowWeaponOneHandMenu(RE::TESObjectREFR* a_source);
    void ShowWeaponTwoHandMenu(RE::TESObjectREFR* a_source);
    void ShowWeaponStaffMenu(RE::TESObjectREFR* a_source);
    void ShowGemstoneMenu(RE::TESObjectREFR* a_source);
    void ShowTreasureMenu(RE::TESObjectREFR* a_source);
    void ShowStolenMenu(RE::TESObjectREFR* a_source);
    void ShowFollowerMenu(RE::TESObjectREFR* a_source);
    void ShowPotionMenu(RE::TESObjectREFR* a_source);
    void ShowPotionPositiveMenu(RE::TESObjectREFR* a_source);
    void ShowPotionNegativeMenu(RE::TESObjectREFR* a_source);
    void ShowPotionRestoreMenu(RE::TESObjectREFR* a_source);
    void ShowSmithMenu(RE::TESObjectREFR* a_source);
    void ShowSmithCraftMenu(RE::TESObjectREFR* a_source);
    void ShowSmithSmeltMenu(RE::TESObjectREFR* a_source);
    void ShowSmithTanMenu(RE::TESObjectREFR* a_source);
    void ShowSmithConstructMenu(RE::TESObjectREFR* a_source);
};