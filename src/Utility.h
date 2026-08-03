#pragma once

#include "Stores.h"
#include "ItemClass.h"

class Utility {
public:
    using ContainerAction = std::function<void(RE::TESObjectREFR*)>;

    struct ContainerChoice {
        std::string name;
        RE::FormID formID{ 0 };
    };

    static Utility* GetSingleton() {
        static Utility playerStatus;
        return &playerStatus; 
    }

    // Category Maps
    std::unordered_set<RE::FormID> SmithingMap;
    std::unordered_set<RE::FormID> SmeltingMap;
    std::unordered_set<RE::FormID> TanningMap;
    std::unordered_set<RE::FormID> ConstructionMap;
    std::unordered_set<RE::FormID> RawFoodMap;
    std::unordered_set<RE::FormID> CookingMap;

    // Load Forms
    void LoadContainers([[maybe_unused]] RE::TESDataHandler* dataHandler);
    void LoadAllForms();
    void CacheTemperRecipes([[maybe_unused]] RE::TESDataHandler* dataHandler);

    bool FoundRestore(RE::AlchemyItem* a_potion);

    // Container Functions
    void AddContainer(RE::TESDataHandler* a_data, Container a_name, RE::FormID a_formid, ContainerGroup a_group, ContainerAction a_action = {});
    RE::TESObjectREFR* GetContainer(Container name);
    [[nodiscard]] std::vector<ContainerChoice> GetContainerChoices() const;
    [[nodiscard]] std::vector<ContainerChoice> GetVaultChoices() const;
    [[nodiscard]] std::vector<ContainerChoice> GetArchiveChoices() const;
    [[nodiscard]] bool RunContainerAction(const RE::TESObjectREFR* container, RE::TESObjectREFR* source) const;

private:
    struct ContainerEntry {
        RE::TESObjectREFR* container{ nullptr };
        ContainerGroup a_group;
        ContainerAction action;
    };

    const std::string_view pluginSkyrim = "Skyrim.esm";
    const std::string_view pluginUpdate = "Update.esm";
    const std::string_view pluginDawnguard = "Dawnguard.esm";
    const std::string_view pluginHeathfire = "HearthFires.esm";
    const std::string_view pluginCampfire = "ccqdrsse002-firewood.esl";
    const std::string_view pluginArcaneVault = "ArcaneVault-LinkedStorage.esp";

    // Construction Keywords
    RE::BGSKeyword* kCarpenter;
    RE::BGSKeyword* kForge;
    RE::BGSKeyword* kTanning;
    RE::BGSKeyword* kSmelting;
    RE::BGSKeyword* kCookPot;
    RE::BGSKeyword* kOven;
    RE::BGSKeyword* kCampfire;
    RE::BGSKeyword* kGrain;

    // Restore Effects
    RE::EffectSetting* RestoreHealth;
    RE::EffectSetting* RestoreMagicka;
    RE::EffectSetting* RestoreStamina;

    // Containers
    std::unordered_map<Container,ContainerEntry> containerMap;

    template <class T>
    T* LookupForm(RE::TESDataHandler* dataHandler, RE::FormID formID, std::string_view plugin, std::string_view name, bool required = true);
};
