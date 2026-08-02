#include "Utility.h"


void Utility::AddContainer(RE::TESDataHandler* a_data, Container a_name, RE::FormID a_formid, ContainerGroup a_group, ContainerAction a_action) {
    auto* form = a_data->LookupForm(a_formid, pluginArcaneVault);
    if (!form) {
        logger::error("Could not find form {:03X} in {}", a_formid, pluginArcaneVault);
        return;
    }

    auto* container = form->As<RE::TESObjectREFR>();
    if (!container) {
        logger::error("Form {:08X} is not a TESObjectREFR", form->GetFormID());
        return;
    }

    containerMap.insert_or_assign(std::move(a_name), ContainerEntry{ container, a_group, std::move(a_action) });
}

RE::TESObjectREFR* Utility::GetContainer(Container name) {
    const auto it = containerMap.find(name);
    return it != containerMap.end() ? it->second.container : nullptr;
}

bool Utility::RunContainerAction(const RE::TESObjectREFR* container, RE::TESObjectREFR* source) const {
    if (!container || !source) return false;

    for (const auto& [name, entry] : containerMap) {
        if (entry.container && entry.container->GetFormID() == container->GetFormID() && entry.action) {
            entry.action(source);
            return true;
        }
    }

    return false;
}

std::vector<Utility::ContainerChoice> Utility::GetContainerChoices() const {
    std::vector<ContainerChoice> choices;
    choices.reserve(containerMap.size());

    for (const auto& [name, entry] : containerMap) {
        if (!entry.container)
            continue;

        choices.push_back(ContainerChoice{
            .name = entry.container->GetDisplayFullName(),
            .formID = entry.container->GetFormID()
        });
    }

    std::ranges::sort(choices, [](const ContainerChoice& lhs, const ContainerChoice& rhs) {
        return lhs.name < rhs.name;
    });

    return choices;
}

std::vector<Utility::ContainerChoice> Utility::GetVaultChoices() const {
    std::vector<ContainerChoice> choices;
    choices.reserve(containerMap.size());

    for (const auto& [name, entry] : containerMap) {
        if (!entry.container || entry.a_group != ContainerGroup::Vault) continue;
        choices.push_back(ContainerChoice{
            .name = entry.container->GetDisplayFullName(),
            .formID = entry.container->GetFormID()
        });
    }

    std::ranges::sort(choices, [](const ContainerChoice& lhs, const ContainerChoice& rhs) {
        return lhs.name < rhs.name;
    });

    return choices;
}

std::vector<Utility::ContainerChoice> Utility::GetArchiveChoices() const {
    std::vector<ContainerChoice> choices;
    choices.reserve(containerMap.size());

    for (const auto& [name, entry] : containerMap) {
        if (!entry.container || entry.a_group != ContainerGroup::Archive) continue;
        choices.push_back(ContainerChoice{
            .name = entry.container->GetDisplayFullName(),
            .formID = entry.container->GetFormID()
        });
    }

    std::ranges::sort(choices, [](const ContainerChoice& lhs, const ContainerChoice& rhs) {
        return lhs.name < rhs.name;
    });

    return choices;
}

bool Utility::FoundRestore(RE::AlchemyItem* a_potion) {
    // Loop through all of the magic effects on the potion
    for (auto& eEffect : a_potion->effects) {
        RE::FormID effectFormID = eEffect->baseEffect->GetFormID();

        // Is it a restor effect
        if (effectFormID == RestoreHealth->formID || effectFormID == RestoreMagicka->formID || effectFormID == RestoreStamina->formID)
            return true;
    }

    return false;
}

template <class T>
T* Utility::LookupForm(RE::TESDataHandler* dataHandler, RE::FormID formID, std::string_view plugin, std::string_view name, bool required) {
    auto* form = dataHandler->LookupForm(formID, plugin);
    auto* result = form ? form->As<T>() : nullptr;

    if (!result) {
        if (required)
            logger::critical("Failed to load {}", name);
        else
            logger::warn("Failed to load {}", name);
    }

    return result;
}

void Utility::LoadAllForms() {
    logger::info("Loading all forms.");
    const auto dataHandler = RE::TESDataHandler::GetSingleton();


    // Construction Keywords
    kForge = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x088105), pluginSkyrim, "Forge keyword");
    kTanning = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x07866A), pluginSkyrim, "Tanning Rack keyword");
    kSmelting = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x0A5CCE), pluginSkyrim, "Smelter keyword");
    kCarpenter = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x003015), pluginHeathfire, "Carpenter keyword");
    kCookPot = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x0A5CB3), pluginSkyrim, "Cooking Pot keyword");
    kOven = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x0117F7), pluginHeathfire, "Oven keyword");
    kCampfire = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x800), pluginCampfire, "Campfire keyword");
    kGrain = LookupForm<RE::BGSKeyword>(dataHandler, RE::FormID(0x09C6DE), pluginSkyrim, "Grain keyword");

    // Restore Effects
    RestoreHealth = LookupForm<RE::EffectSetting>(dataHandler, RE::FormID(0x3EB15), pluginSkyrim, "Restore Health Effect");
    RestoreMagicka = LookupForm<RE::EffectSetting>(dataHandler, RE::FormID(0x3EB17), pluginSkyrim, "Restore Magicka Effect");
    RestoreStamina = LookupForm<RE::EffectSetting>(dataHandler, RE::FormID(0x3EB16), pluginSkyrim, "Restore Stamina Effect");

    // Storage Containers
    LoadContainers(dataHandler);
    logger::info("All forms are loaded.");
}

void Utility::LoadContainers([[maybe_unused]] RE::TESDataHandler* dataHandler) {
    // Sorting Chest
    AddContainer(dataHandler, Container::Master, RE::FormID(0xD93), ContainerGroup::Special, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->VaultMenu(a_source); }); 
    AddContainer(dataHandler, Container::Sort, RE::FormID(0xDB9), ContainerGroup::Special, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SortingMenu(a_source); }); 

    // Smithing
    AddContainer(dataHandler, Container::Smithing, RE::FormID(0xDA7), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SmithMenu(a_source); }); 
    AddContainer(dataHandler, Container::Crafting, RE::FormID(0xDA8), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SmithCraftMenu(a_source); }); 
    AddContainer(dataHandler, Container::Smelting, RE::FormID(0xDAA), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SmithSmeltMenu(a_source); }); 
    AddContainer(dataHandler, Container::Tanning, RE::FormID(0xDAB), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SmithTanMenu(a_source); }); 
    AddContainer(dataHandler, Container::Construction, RE::FormID(0xDA9), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SmithConstructMenu(a_source); });
    
    // Alchemy
    AddContainer(dataHandler, Container::Alchemy, RE::FormID(0xD94), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->AlchemyMenu(a_source); }); 
    AddContainer(dataHandler, Container::Concoction, RE::FormID(0xDA3), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->PotionMenu(a_source); });
    AddContainer(dataHandler, Container::Potion, RE::FormID(0xDA4), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->PotionPositiveMenu(a_source); });
    AddContainer(dataHandler, Container::Poison, RE::FormID(0xDA5), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->PotionNegativeMenu(a_source); });
    AddContainer(dataHandler, Container::Restorative, RE::FormID(0xDA6), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->PotionRestoreMenu(a_source); });
    AddContainer(dataHandler, Container::Soulgem, RE::FormID(0xDAC), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->SoulgemMenu(a_source); });
    
    // Food
    AddContainer(dataHandler, Container::Food, RE::FormID(0xD9D), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->FoodMenu(a_source); });
    AddContainer(dataHandler, Container::Reagent, RE::FormID(0xDA0), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->FoodSaltMenu(a_source); });
    AddContainer(dataHandler, Container::RawFood, RE::FormID(0xD9F), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->FoodRawMenu(a_source); });
    AddContainer(dataHandler, Container::CookedFood, RE::FormID(0xD9E), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->FoodCookedMenu(a_source); });
    
    // Weapons
    AddContainer(dataHandler, Container::Weapon, RE::FormID(0xDAF), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->WeaponMenu(a_source); });
    AddContainer(dataHandler, Container::Archery, RE::FormID(0xDB0), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->WeaponArcheryMenu(a_source); });
    AddContainer(dataHandler, Container::Staff, RE::FormID(0xDB3), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->WeaponStaffMenu(a_source); });
    AddContainer(dataHandler, Container::OneHand, RE::FormID(0xDB1), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->WeaponOneHandMenu(a_source); });
    AddContainer(dataHandler, Container::TwoHand, RE::FormID(0xDB2), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->WeaponTwoHandMenu(a_source); });
    
    // Armor
    AddContainer(dataHandler, Container::Armor, RE::FormID(0xD95), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->ArmorMenu(a_source); });
    AddContainer(dataHandler, Container::HeavyArmor, RE::FormID(0xD97), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->ArmorHeavyMenu(a_source); });
    AddContainer(dataHandler, Container::LightArmor, RE::FormID(0xD98), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->ArmorLightMenu(a_source); });
    AddContainer(dataHandler, Container::Shield, RE::FormID(0xD99), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->ArmorShieldMenu(a_source); });
    AddContainer(dataHandler, Container::Clothing, RE::FormID(0xD96), ContainerGroup::Archive, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->ArmorClothingMenu(a_source); });

    // Books and Scrolls
    AddContainer(dataHandler, Container::Book, RE::FormID(0xD9A), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->BookMenu(a_source); });
    AddContainer(dataHandler, Container::Scroll, RE::FormID(0xD9B), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->ScrollMenu(a_source); });
    
    // Misc
    AddContainer(dataHandler, Container::Gemstone, RE::FormID(0xDA2), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->GemstoneMenu(a_source); });
    AddContainer(dataHandler, Container::Treasure, RE::FormID(0xDAD), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->TreasureMenu(a_source); });
    AddContainer(dataHandler, Container::Stolen, RE::FormID(0xDAE), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->StolenMenu(a_source); });
    AddContainer(dataHandler, Container::Follower, RE::FormID(0xDA1), ContainerGroup::Vault, [](RE::TESObjectREFR* a_source) { Stores::GetSingleton()->FollowerMenu(a_source); });
}

void Utility::CacheTemperRecipes() {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;
    
    // Get all constructible objects
    const auto recipes = dataHandler->GetFormArray(RE::FormType::ConstructibleObject);

    // Loop through all the recipes
    for (auto* form : recipes) {
        auto* recipe = form ? form->As<RE::BGSConstructibleObject>() : nullptr;
        if (!recipe || !recipe->createdItem || !recipe->benchKeyword)
            continue;

        auto* keyword = recipe->benchKeyword;
        std::string_view editorID = keyword->formEditorID.c_str();

        // Smithing Items
        if (keyword == kForge) {
            recipe->requiredItems.ForEachContainerObject([this](RE::ContainerObject& entry) {
                if (entry.obj) this->SmithingMap.insert(entry.obj->GetFormID());
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        // Smelting Items
        if (keyword == kSmelting) {
            recipe->requiredItems.ForEachContainerObject([this](RE::ContainerObject& entry) {
                if (entry.obj) this->SmeltingMap.insert(entry.obj->GetFormID());
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        // Tanning Items
        if (keyword == kTanning) {
            recipe->requiredItems.ForEachContainerObject([this](RE::ContainerObject& entry) {
                if (entry.obj) this->TanningMap.insert(entry.obj->GetFormID());
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        // Carpenter Items
        if (keyword == kCarpenter || editorID.contains("BYOHBuilding") || editorID.contains("BYOHHouse")) {
            recipe->requiredItems.ForEachContainerObject([this](RE::ContainerObject& entry) {
                if (entry.obj) this->ConstructionMap.insert(entry.obj->GetFormID());
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        // Raw Food
        if (keyword == kCookPot || keyword == kOven || keyword == kCampfire) {
            recipe->requiredItems.ForEachContainerObject([this](RE::ContainerObject& entry) {
                if (entry.obj) this->RawFoodMap.insert(entry.obj->GetFormID());
                return RE::BSContainer::ForEachResult::kContinue;
            });

            CookingMap.insert(recipe->createdItem->formID);
        }
    }

    logger::info("Loaded: {} Smithing Items", SmithingMap.size());
    logger::info("Loaded: {} Smelting Items", SmeltingMap.size());
    logger::info("Loaded: {} Tanning Items", TanningMap.size());
    logger::info("Loaded: {} Construction Items", ConstructionMap.size());
    logger::info("Loaded: {} Cooking Items", CookingMap.size());
}