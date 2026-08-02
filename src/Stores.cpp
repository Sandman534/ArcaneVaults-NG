#pragma once
#include "Stores.h"
#include "Events.h"
#include "ContainerMenu.h"
#include "RedirectManager.h"

#include <chrono>
#include <thread>

namespace {
    using OpenState = RE::BGSOpenCloseForm::OPEN_STATE;

    struct OpenStatePoll
    {
        RE::ObjectRefHandle handle;
        OpenState desiredState;
        Stores::AnimationCallback callback;
        std::chrono::steady_clock::time_point deadline;
    };

    void PollForOpenState(std::shared_ptr<OpenStatePoll> a_poll)
    {
        // AddTask can drain tasks added by another task during the same frame. Delay
        // off-thread before returning to the game thread so animation frames advance.
        std::thread([a_poll = std::move(a_poll)]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));

            SKSE::GetTaskInterface()->AddTask([a_poll = std::move(a_poll)]() mutable {
                auto source = a_poll->handle.get();
                if (!source) {
                    logger::warn("Stopped waiting for storage animation because its reference is no longer available");
                    return;
                }

                if (RE::BGSOpenCloseForm::GetOpenState(source.get()) == a_poll->desiredState) {
                    if (a_poll->callback)
                        a_poll->callback(source.get());
                    return;
                }

                if (std::chrono::steady_clock::now() >= a_poll->deadline) {
                    logger::warn("Timed out waiting for storage animation on {:08X}", source->GetFormID());
                    // Do not leave activation permanently swallowed if a model does
                    // not expose a usable open/close state.
                    if (a_poll->callback)
                        a_poll->callback(source.get());
                    return;
                }

                PollForOpenState(std::move(a_poll));
            });
        }).detach();
    }

    void WaitForOpenState(RE::TESObjectREFR* a_source, OpenState a_state, Stores::AnimationCallback a_callback)
    {
        if (!a_callback)
            return;

        if (RE::BGSOpenCloseForm::GetOpenState(a_source) == a_state) {
            a_callback(a_source);
            return;
        }

        PollForOpenState(std::make_shared<OpenStatePoll>(
            a_source->GetHandle(),
            a_state,
            std::move(a_callback),
            std::chrono::steady_clock::now() + std::chrono::seconds(5)));
    }
}

void Stores::ShowDynamicMessageBox(std::string_view a_message, std::vector<MenuOption> a_options, std::int32_t a_cancelOption) {
    if (a_options.empty()) return;

    // Start the strings
    const auto* strings = RE::InterfaceStrings::GetSingleton();
    auto* factoryManager = RE::MessageDataFactoryManager::GetSingleton();
    if (!strings || !factoryManager) return;

    // Setup the message box
    const auto* creator = factoryManager->GetCreator<RE::MessageBoxData>(strings->messageBoxData);
    if (!creator) return;

    // Create the data
    auto* data = creator->Create();
    if (!data) return;

    // Prepare actions
    std::vector<MenuAction> actions;
    actions.reserve(a_options.size());

    // Create the body text
    data->bodyText = a_message.data();
    for (auto& option : a_options) {
        data->buttonText.emplace_back(option.text.c_str());
        actions.emplace_back(std::move(option.action));
    }

    // Set menu data
    data->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new MenuCallback(std::move(actions)));
    data->type = 0;
    data->cancelOptionIndex = a_cancelOption;
    data->menuDepth = 4;
    data->optionIndexOffset = 0;
    data->useHtml = true;
    data->verticalButtons = false;
    data->isCancellable = a_cancelOption >= 0;
    data->QueueMessage();
}

//===================================================
// Storage Functions
//===================================================
void Stores::PlayerToStore(ItemCategory a_category, Container a_chest, int i_count) {
    auto* utility = Utility::GetSingleton();
    auto* chest = utility->GetContainer(a_chest);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !chest) return;

    const auto inventory = player->GetInventory();
    for (const auto& [item, data] : inventory) {
        if (!item || data.first <= 0) continue;

        // Ignore Quest, Favorited and Worn items
        if (data.second->IsQuestObject() || data.second->IsFavorited() || data.second->IsWorn()) continue;

        const auto categories = ClassifyItem(item);
        if (!HasCategory(categories, a_category)) continue;

        // If the input is zero, move all
        if (i_count == 0)
            player->RemoveItem(item, 9999, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, chest);
        else
            player->RemoveItem(item, i_count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, chest);

        
    }
}

void Stores::StoreToPlayer(ItemCategory a_category, Container a_chest, int i_count) {
    auto* utility = Utility::GetSingleton();
    auto* chest = utility->GetContainer(a_chest);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !chest) return;

    const auto inventory = chest->GetInventory();
    for (const auto& [item, data] : inventory) {
        if (!item || data.first <= 0) continue;

        const auto categories = ClassifyItem(item);
        if (!HasCategory(categories, a_category)) continue;

        // If the input is zero, move all
        if (i_count == 0)
            chest->RemoveItem(item, 9999, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, player);
        else
            chest->RemoveItem(item, i_count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, player);
    }
}

void Stores::StoreToStore(ItemCategory a_category, Container a_source, Container a_destination) {
    auto* utility = Utility::GetSingleton();
    auto* source = utility->GetContainer(a_source);
    auto* destination = utility->GetContainer(a_destination);
    if (!source || !destination) return;

    const auto inventory = source->GetInventory();
    for (const auto& [item, data] : inventory) {
        if (!item || data.first <= 0) continue;

        const auto categories = ClassifyItem(item);
        if (!HasCategory(categories, a_category)) continue;
        
        source->RemoveItem(item, 9999, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, destination);
    }
}

void Stores::AllToStore(Container a_source, Container a_destination) {
    auto* utility = Utility::GetSingleton();
    auto* source = utility->GetContainer(a_source);
    auto* destination = utility->GetContainer(a_destination);
    if (!source || !destination || a_source == a_destination) return;

    auto* inventory = source->GetInventoryChanges();
    if (!inventory) return;

    inventory->RemoveAllItems(source, destination, false, false, false);
}

void Stores::AllToPlayer(Container a_source) {
    auto* utility = Utility::GetSingleton();
    auto* source = utility->GetContainer(a_source);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!source || !player) return;

    auto* inventory = source->GetInventoryChanges();
    if (!inventory) return;

    inventory->RemoveAllItems(source, player, false, false, false);
}

//===================================================
// Storage Animations
//===================================================
void Stores::OpenStorageMenu(Container a_chest, RE::TESObjectREFR* a_source, std::function<void(RE::TESObjectREFR*)> a_onClose) {
    if (!a_source || !a_onClose) return;

    auto* chest = Utility::GetSingleton()->GetContainer(a_chest);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!chest || !player) return;

    const auto sourceHandle = a_source->GetHandle();
    auto returnToMenu = [sourceHandle, a_onClose = std::move(a_onClose)] {
        if (auto source = sourceHandle.get())
            a_onClose(source.get());
    };

    Events::RunAfterContainerMenuCloses(chest, returnToMenu);

    if (!chest->ActivateRef(player, 0, nullptr, 1, false)) {
        Events::CancelContainerMenuCloseCallback();
        returnToMenu();
    }
}

void Stores::OpenStoreObject(RE::TESObjectREFR* a_source, AnimationCallback a_onComplete) {
    if (!a_source) return;

    const auto openState = RE::BGSOpenCloseForm::GetOpenState(a_source);
    if (openState != RE::BGSOpenCloseForm::OPEN_STATE::kOpen && openState != RE::BGSOpenCloseForm::OPEN_STATE::kOpening) {
        RE::BGSOpenCloseForm::SetOpenState(a_source, true, false);
    }

    WaitForOpenState(a_source, RE::BGSOpenCloseForm::OPEN_STATE::kOpen, std::move(a_onComplete));
}

void Stores::CloseStoreObject(RE::TESObjectREFR* a_source, AnimationCallback a_onComplete) {
    if (!a_source) return;

    const auto openState = RE::BGSOpenCloseForm::GetOpenState(a_source);
    if (openState != RE::BGSOpenCloseForm::OPEN_STATE::kClosed &&
        openState != RE::BGSOpenCloseForm::OPEN_STATE::kClosing) {
        RE::BGSOpenCloseForm::SetOpenState(a_source, false, false);
    }

    WaitForOpenState(a_source, RE::BGSOpenCloseForm::OPEN_STATE::kClosed, std::move(a_onComplete));
}

//===================================================
// Sorting
//===================================================
void Stores::SortItems() {
    // Sort all items in the sorting container
    StoreToStore(ItemCategory::Smithing, Container::Sort, Container::Smithing);
    StoreToStore(ItemCategory::Food, Container::Sort, Container::Food);
    StoreToStore(ItemCategory::Ingredient, Container::Sort, Container::Alchemy);
    StoreToStore(ItemCategory::Soulgem, Container::Sort, Container::Soulgem);
    StoreToStore(ItemCategory::Scroll, Container::Sort, Container::Scroll);
    StoreToStore(ItemCategory::Book, Container::Sort, Container::Book);
    StoreToStore(ItemCategory::Treasure, Container::Sort, Container::Treasure);
    StoreToStore(ItemCategory::Armor, Container::Sort, Container::Armor);
    StoreToStore(ItemCategory::Weapon, Container::Sort, Container::Weapon);
    StoreToStore(ItemCategory::Concoction, Container::Sort, Container::Concoction);
    RE::DebugNotification("All items sorted");
}

void Stores::OffloadItems() {
    // Only take specific items
    PlayerToStore(ItemCategory::Smithing, Container::Smithing);
    PlayerToStore(ItemCategory::Ingredient, Container::Alchemy);
    PlayerToStore(ItemCategory::GrandSoulgem, Container::Soulgem);
    PlayerToStore(ItemCategory::Book, Container::Book);
    PlayerToStore(ItemCategory::RawFood, Container::Food);

    RE::DebugNotification("Stashed items");
}

//===================================================
// Vault Menu
//===================================================
void Stores::VaultMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowVaultMenu(a_openSource); });
}

void Stores::ShowVaultMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Offload", [this, sourceHandle] {
        OffloadItems();
        if (auto source = sourceHandle.get()) VaultMenu(source.get());
    }});

    options.push_back({"Auto Sort", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Sort, source.get(), [this](RE::TESObjectREFR* a_source) {
                SortItems();
                VaultMenu(a_source);
            });
    }});

    options.push_back({"Open Vaults", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) VaultDetail1Menu(source.get());
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Arcane Vault", std::move(options), cancelIndex);
}

void Stores::VaultDetail1Menu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowVaultDetail1Menu(a_openSource); });
}

void Stores::ShowVaultDetail1Menu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Smithing", [this, sourceHandle] {
        AllToStore(Container::Crafting, Container::Smithing);
        AllToStore(Container::Smelting, Container::Smithing);
        AllToStore(Container::Tanning, Container::Smithing);
        AllToStore(Container::Construction, Container::Smithing);
        
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Smithing, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail1Menu(a_source);
            });
    }});

    options.push_back({"Alchemy", [this, sourceHandle] {
        AllToStore(Container::Reagent, Container::Alchemy);
        
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Alchemy, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail1Menu(a_source);
            });
    }});

    options.push_back({"Armor", [this, sourceHandle] {
        AllToStore(Container::HeavyArmor, Container::Armor);
        AllToStore(Container::LightArmor, Container::Armor);
        AllToStore(Container::Shield, Container::Armor);
        AllToStore(Container::Clothing, Container::Armor);
        
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Armor, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail1Menu(a_source);
            });
    }});

    options.push_back({"Weapons", [this, sourceHandle] {
        AllToStore(Container::Archery, Container::Weapon);
        AllToStore(Container::OneHand, Container::Weapon);
        AllToStore(Container::TwoHand, Container::Weapon);
        AllToStore(Container::Staff, Container::Weapon);
        
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Weapon, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail1Menu(a_source);
            });
    }});

    options.push_back({"Concoctions", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Concoction, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail1Menu(a_source);
            });
    }});

    options.push_back({"Food", [this, sourceHandle] {
        AllToStore(Container::RawFood, Container::Food);
        AllToStore(Container::CookedFood, Container::Food);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Food, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail1Menu(a_source);
            });
    }});

    options.push_back({"More", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) VaultDetail2Menu(source.get());
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Arcane Vaults", std::move(options), cancelIndex);
}

void Stores::VaultDetail2Menu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowVaultDetail2Menu(a_openSource); });
}

void Stores::ShowVaultDetail2Menu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Books", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Book, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail2Menu(a_source);
            });
    }});

    options.push_back({"Scrolls", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Scroll, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail2Menu(a_source);
            });
    }});

    options.push_back({"Soulgems", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Soulgem, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail2Menu(a_source);
            });
    }});

    options.push_back({"Stolen", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Stolen, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail2Menu(a_source);
            });
    }});

    options.push_back({"Treasure", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Treasure, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail2Menu(a_source);
            });
    }});

    options.push_back({"Follower", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Follower, source.get(), [this](RE::TESObjectREFR* a_source) {
                ShowVaultDetail2Menu(a_source);
            });
    }});

    options.push_back({"Back", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) VaultDetail1Menu(source.get());
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Arcane Vaults", std::move(options), cancelIndex);
}

//===================================================
// Vault Assignment
//===================================================
void Stores::VaultAssignment(RE::TESObjectREFRPtr a_source) {
    if (!a_source) return;
    auto* utility = Utility::GetSingleton();
    std::vector<MenuOption> options;

    options.push_back({"Assign Master", [utility, a_source] {
        auto* source = a_source.get();
        auto* destination = utility->GetContainer(Container::Master);
        if (destination && destination != source) RedirectManager::GetSingleton()->Add(source, destination);
    }});

    options.push_back({"Assign Sorter", [utility, a_source] {
        auto* source = a_source.get();
        auto* destination = utility->GetContainer(Container::Sort);
        if (destination && destination != source) RedirectManager::GetSingleton()->Add(source, destination);
    }});

    options.push_back({"Assign Vault", [this, a_source] {
        auto choices = Utility::GetSingleton()->GetVaultChoices();
        ContainerSelectionState::GetSingleton()->Begin(std::move(a_source), std::move(choices));
        RE::UIMessageQueue::GetSingleton()->AddMessage(ContainerListMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }});

    options.push_back({"Assign Archive", [this, a_source] {
        auto choices = Utility::GetSingleton()->GetArchiveChoices();
        ContainerSelectionState::GetSingleton()->Begin(std::move(a_source), std::move(choices));
        RE::UIMessageQueue::GetSingleton()->AddMessage(ContainerListMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }});

    options.push_back({"Remove Assignment", [this, a_source] {
        auto* source = a_source.get();
        if (source) RedirectManager::GetSingleton()->Remove(source);
    }});

    options.push_back({"Exit", [] { }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Alchemy Vault", std::move(options), cancelIndex);
}

//===================================================
// Auto Sort Menu
//===================================================
void Stores::SortingMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowSortingMenu(a_openSource); });
}

void Stores::ShowSortingMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Sort, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            SortItems();
    });
}

//===================================================
// *Alchemy Menus
//===================================================
void Stores::AlchemyMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowAlchemyMenu(a_openSource); });
}

void Stores::ShowAlchemyMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Ingredient, Container::Alchemy);
        RE::DebugNotification("All ingredients stashed");
        if (auto source = sourceHandle.get()) AlchemyMenu(source.get());
    }});

    options.push_back({"Keep One", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Ingredient, Container::Alchemy);
        StoreToPlayer(ItemCategory::Ingredient, Container::Alchemy, 1);
        RE::DebugNotification("Most ingredients stashed");
        if (auto source = sourceHandle.get()) AlchemyMenu(source.get());
    }});

    options.push_back({"Take Cooking Items", [this, sourceHandle] {
        StoreToPlayer(ItemCategory::Reagent, Container::Alchemy);
        RE::DebugNotification("Retrieved cooking items");
        if (auto source = sourceHandle.get()) AlchemyMenu(source.get());
    }});

    options.push_back({"Open Vault", [this, sourceHandle] {
        AllToStore(Container::Reagent, Container::Alchemy);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Alchemy, source.get(), [this](RE::TESObjectREFR* a_source) {
                AlchemyMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Alchemy Vault", std::move(options), cancelIndex);
}

void Stores::SoulgemMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowSoulgemMenu(a_openSource); });
}

void Stores::ShowSoulgemMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Soulgem, Container::Soulgem);
        RE::DebugNotification("All soulgems stashed");
        if (auto source = sourceHandle.get()) SoulgemMenu(source.get());
    }});

    options.push_back({"Store Grand Souls", [this, sourceHandle] {
        PlayerToStore(ItemCategory::GrandSoulgem, Container::Soulgem);
        RE::DebugNotification("Grand souls stashed");
        if (auto source = sourceHandle.get()) SoulgemMenu(source.get());
    }});

    options.push_back({"Grab Empty Gems", [this, sourceHandle] {
        StoreToPlayer(ItemCategory::EmptySoulgem, Container::Soulgem);
        RE::DebugNotification("Retrieved cooking items");
        if (auto source = sourceHandle.get()) SoulgemMenu(source.get());
    }});

    options.push_back({"Open Vault", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Soulgem, source.get(), [this](RE::TESObjectREFR* a_source) {
                SoulgemMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Soulgem Vault", std::move(options), cancelIndex);
}

//===================================================
// *Potion Menus
//===================================================
void Stores::PotionMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowPotionMenu(a_openSource); });
}

void Stores::ShowPotionMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Concoction, Container::Concoction);
        RE::DebugNotification("All concoctions stashed");
        if (auto source = sourceHandle.get()) PotionMenu(source.get());
    }});

    options.push_back({"Potion Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Potion, Container::Concoction, Container::Potion);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Potion, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Potion, Container::Concoction);
                PotionMenu(a_source);
            });
    }});

    options.push_back({"Posion Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Poison, Container::Concoction, Container::Poison);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Poison, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Poison, Container::Concoction);
                PotionMenu(a_source);
            });
    }});

    options.push_back({"Restore Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Restorative, Container::Concoction, Container::Restorative);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Restorative, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Restorative, Container::Concoction);
                PotionMenu(a_source);
            });
    }});

    options.push_back({"Concoction Vault", [this, sourceHandle] {
        AllToStore(Container::Potion, Container::Concoction);
        AllToStore(Container::Poison, Container::Concoction);
        AllToStore(Container::Restorative, Container::Concoction);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Concoction, source.get(), [this](RE::TESObjectREFR* a_source) {
                PotionMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Concoction Vault", std::move(options), cancelIndex);
}

void Stores::PotionPositiveMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowPotionPositiveMenu(a_openSource); });
}

void Stores::ShowPotionPositiveMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Potion, Container::Concoction, Container::Potion);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Potion, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Potion, Container::Concoction);
    });
}

void Stores::PotionNegativeMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowPotionNegativeMenu(a_openSource); });
}

void Stores::ShowPotionNegativeMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Poison, Container::Concoction, Container::Poison);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Poison, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Poison, Container::Concoction);
    });
}

void Stores::PotionRestoreMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowPotionRestoreMenu(a_openSource); });
}

void Stores::ShowPotionRestoreMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Restorative, Container::Concoction, Container::Restorative);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Restorative, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Restorative, Container::Concoction);
    });
}

//===================================================
// *Book Menus
//===================================================
void Stores::BookMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowBookMenu(a_openSource); });
}

void Stores::ShowBookMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Book, Container::Book);
        RE::DebugNotification("All books stashed");
        if (auto source = sourceHandle.get()) BookMenu(source.get());
    }});

    options.push_back({"Store One Copy", [this, sourceHandle] {
        AllToPlayer(Container::Book);
        PlayerToStore(ItemCategory::Book, Container::Book, 1);
        RE::DebugNotification("First editions catalogued");
        if (auto source = sourceHandle.get()) BookMenu(source.get());
    }});

    options.push_back({"Gather Spell Tomes", [this, sourceHandle] {
        StoreToPlayer(ItemCategory::SpellTome, Container::Book);
        RE::DebugNotification("Tomes collected");
        if (auto source = sourceHandle.get()) BookMenu(source.get());
    }});

    options.push_back({"Open Vault", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Book, source.get(), [this](RE::TESObjectREFR* a_source) {
                BookMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Book Vault", std::move(options), cancelIndex);
}

void Stores::ScrollMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowScrollMenu(a_openSource); });
}

void Stores::ShowScrollMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Book, Container::Book);
        RE::DebugNotification("All scrolls stashed");
        if (auto source = sourceHandle.get()) ScrollMenu(source.get());
    }});

    options.push_back({"Keep One", [this, sourceHandle] {
        AllToPlayer(Container::Book);
        PlayerToStore(ItemCategory::Book, Container::Book, 1);
        RE::DebugNotification("Extra scrolls stashed");
        if (auto source = sourceHandle.get()) ScrollMenu(source.get());
    }});

    options.push_back({"Keep Three", [this, sourceHandle] {
        AllToPlayer(Container::Book);
        PlayerToStore(ItemCategory::Book, Container::Book, 3);
        RE::DebugNotification("Extra scrolls stashed");
        if (auto source = sourceHandle.get()) ScrollMenu(source.get());
    }});

    options.push_back({"Open Vault", [this, sourceHandle] {
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Book, source.get(), [this](RE::TESObjectREFR* a_source) {
                ScrollMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Scroll Vault", std::move(options), cancelIndex);
}

//===================================================
// *Smith Menus
//===================================================
void Stores::SmithMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowFoodMenu(a_openSource); });
}

void Stores::ShowSmithMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Smithing, Container::Smithing);
        RE::DebugNotification("All items stashed");
        if (auto source = sourceHandle.get()) FoodMenu(source.get());
    }});

    options.push_back({"Smithing Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Crafting, Container::Smithing, Container::Crafting);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Crafting, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Crafting, Container::Smithing);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Smelting Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Smelting, Container::Smithing, Container::Smelting);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Smelting, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Smelting, Container::Smithing);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Tanning Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Tanning, Container::Smithing, Container::Tanning);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Tanning, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Tanning, Container::Smithing);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Construction Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Construction, Container::Smithing, Container::Construction);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Construction, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Construction, Container::Smithing);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"GemStones", [this, sourceHandle] {
        StoreToStore(ItemCategory::Gemstone, Container::Smithing, Container::Gemstone);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Gemstone, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Gemstone, Container::Smithing);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Smithing Vault", [this, sourceHandle] {
        AllToStore(Container::Construction, Container::Smithing);
        AllToStore(Container::Crafting, Container::Smithing);
        AllToStore(Container::Smelting, Container::Smithing);
        AllToStore(Container::Tanning, Container::Smithing);
        AllToStore(Container::Gemstone, Container::Smithing);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Smithing, source.get(), [this](RE::TESObjectREFR* a_source) {
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Crafting Vault", std::move(options), cancelIndex);
}

void Stores::SmithCraftMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowSmithCraftMenu(a_openSource); });
}

void Stores::ShowSmithCraftMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Crafting, Container::Smithing, Container::Crafting);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Crafting, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Crafting, Container::Smithing);
    });
}

void Stores::SmithSmeltMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowSmithSmeltMenu(a_openSource); });
}

void Stores::ShowSmithSmeltMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Smelting, Container::Smithing, Container::Smelting);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Smelting, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Smelting, Container::Smithing);
    });
}

void Stores::SmithTanMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowSmithTanMenu(a_openSource); });
}

void Stores::ShowSmithTanMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Tanning, Container::Smithing, Container::Tanning);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Tanning, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Tanning, Container::Smithing);
    });
}

void Stores::SmithConstructMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowSmithConstructMenu(a_openSource); });
}

void Stores::ShowSmithConstructMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Construction, Container::Smithing, Container::Construction);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Construction, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Construction, Container::Smithing);
    });
}

//===================================================
// *Food Menus
//===================================================
void Stores::FoodMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowFoodMenu(a_openSource); });
}

void Stores::ShowFoodMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Food, Container::Food);
        RE::DebugNotification("All foods stashed");
        if (auto source = sourceHandle.get()) FoodMenu(source.get());
    }});

    options.push_back({"Store Raw", [this, sourceHandle] {
        PlayerToStore(ItemCategory::RawFood, Container::Food);
        RE::DebugNotification("Raw foods stashed");
        if (auto source = sourceHandle.get()) FoodMenu(source.get());
    }});

    options.push_back({"Food Vault", [this, sourceHandle] {
        AllToStore(Container::RawFood, Container::Food);
        AllToStore(Container::CookedFood, Container::Food);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Food, source.get(), [this](RE::TESObjectREFR* a_source) {
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Cooked Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::CookedFood, Container::Food, Container::CookedFood);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::CookedFood, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::CookedFood, Container::Food);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Raw Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::RawFood, Container::Food, Container::RawFood);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::RawFood, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::RawFood, Container::Food);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Spice Archive", [this, sourceHandle] {
        StoreToStore(ItemCategory::Reagent, Container::Alchemy, Container::Reagent);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::RawFood, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Reagent, Container::Alchemy);
                FoodMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Food Vault", std::move(options), cancelIndex);
}

void Stores::FoodCookedMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowFoodCookedMenu(a_openSource); });
}

void Stores::ShowFoodCookedMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::CookedFood, Container::Food, Container::CookedFood);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::CookedFood, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::CookedFood, Container::Food);
    });
}

void Stores::FoodRawMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowFoodRawMenu(a_openSource); });
}

void Stores::ShowFoodRawMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::RawFood, Container::Food, Container::RawFood);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::RawFood, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::RawFood, Container::Food);
    });
}

void Stores::FoodSaltMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowFoodSaltMenu(a_openSource); });
}

void Stores::ShowFoodSaltMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Reagent, Container::Alchemy, Container::Reagent);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Reagent, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Reagent, Container::Alchemy);
    });
}

//===================================================
// *Armor Menus
//===================================================
void Stores::ArmorMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowArmorMenu(a_openSource); });
}

void Stores::ShowArmorMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::HeavyArmor, Container::Armor);
        PlayerToStore(ItemCategory::LightArmor, Container::Armor);
        PlayerToStore(ItemCategory::Clothing, Container::Armor);
        PlayerToStore(ItemCategory::Shield, Container::Armor);
        RE::DebugNotification("All armor stashed");
        if (auto source = sourceHandle.get()) ArmorMenu(source.get());
    }});

    options.push_back({"Heavy Armor", [this, sourceHandle] {
        StoreToStore(ItemCategory::HeavyArmor, Container::Armor, Container::HeavyArmor);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::HeavyArmor, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::HeavyArmor, Container::Armor);
                ArmorMenu(a_source);
            });
    }});

    options.push_back({"Light Armor", [this, sourceHandle] {
        StoreToStore(ItemCategory::LightArmor, Container::Armor, Container::LightArmor);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::LightArmor, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::LightArmor, Container::Armor);
                ArmorMenu(a_source);
            });
    }});

    options.push_back({"Clothing", [this, sourceHandle] {
        StoreToStore(ItemCategory::Clothing, Container::Armor, Container::Clothing);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Clothing, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Clothing, Container::Armor);
                ArmorMenu(a_source);
            });
    }});

    options.push_back({"Shields", [this, sourceHandle] {
        StoreToStore(ItemCategory::Shield, Container::Armor, Container::Shield);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Shield, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Shield, Container::Armor);
                ArmorMenu(a_source);
            });
    }});

    options.push_back({"Unclassified", [this, sourceHandle] {
        AllToStore(Container::HeavyArmor, Container::Armor);
        AllToStore(Container::LightArmor, Container::Armor);
        AllToStore(Container::Shield, Container::Armor);
        AllToStore(Container::Clothing, Container::Armor);

        StoreToStore(ItemCategory::HeavyArmor, Container::Armor, Container::HeavyArmor);
        StoreToStore(ItemCategory::LightArmor, Container::Armor, Container::LightArmor);
        StoreToStore(ItemCategory::Clothing, Container::Armor, Container::Clothing);
        StoreToStore(ItemCategory::Shield, Container::Armor, Container::Shield);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Armor, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::HeavyArmor, Container::Armor);
                AllToStore(Container::LightArmor, Container::Armor);
                AllToStore(Container::Shield, Container::Armor);
                AllToStore(Container::Clothing, Container::Armor);
                ArmorMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Armor Vault", std::move(options), cancelIndex);
}

void Stores::ArmorLightMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowArmorLightMenu(a_openSource); });
}

void Stores::ShowArmorLightMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::LightArmor, Container::Armor, Container::LightArmor);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::LightArmor, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::LightArmor, Container::Armor);
    });
}

void Stores::ArmorHeavyMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowArmorHeavyMenu(a_openSource); });
}

void Stores::ShowArmorHeavyMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::HeavyArmor, Container::Armor, Container::HeavyArmor);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::HeavyArmor, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::HeavyArmor, Container::Armor);
    });
}

void Stores::ArmorShieldMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowArmorShieldMenu(a_openSource); });
}

void Stores::ShowArmorShieldMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Shield, Container::Armor, Container::Shield);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Shield, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Shield, Container::Armor);
    });
}

void Stores::ArmorClothingMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowArmorClothingMenu(a_openSource); });
}

void Stores::ShowArmorClothingMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Clothing, Container::Armor, Container::Clothing);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Clothing, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Clothing, Container::Armor);
    });
}

//===================================================
// *Weapons Menus
//===================================================
void Stores::WeaponMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowWeaponMenu(a_openSource); });
}

void Stores::ShowWeaponMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();
    std::vector<MenuOption> options;

    options.push_back({"Store All", [this, sourceHandle] {
        PlayerToStore(ItemCategory::Archery, Container::Weapon);
        PlayerToStore(ItemCategory::OneHand, Container::Weapon);
        PlayerToStore(ItemCategory::TwoHand, Container::Weapon);
        PlayerToStore(ItemCategory::Staff, Container::Weapon);
        RE::DebugNotification("All weapons stashed");
        if (auto source = sourceHandle.get()) WeaponMenu(source.get());
    }});

    options.push_back({"One-Handed", [this, sourceHandle] {
        StoreToStore(ItemCategory::OneHand, Container::Weapon, Container::OneHand);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::OneHand, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::OneHand, Container::Weapon);
                WeaponMenu(a_source);
            });
    }});

    options.push_back({"Two-Handed", [this, sourceHandle] {
        StoreToStore(ItemCategory::TwoHand, Container::Weapon, Container::TwoHand);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::TwoHand, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::TwoHand, Container::Weapon);
                WeaponMenu(a_source);
            });
    }});

    options.push_back({"Archery", [this, sourceHandle] {
        StoreToStore(ItemCategory::Archery, Container::Weapon, Container::Archery);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Archery, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Archery, Container::Weapon);
                WeaponMenu(a_source);
            });
    }});

    options.push_back({"Staffs", [this, sourceHandle] {
        StoreToStore(ItemCategory::Staff, Container::Weapon, Container::Staff);
        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Staff, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Staff, Container::Weapon);
                WeaponMenu(a_source);
            });
    }});

    options.push_back({"Unclassified", [this, sourceHandle] {
        AllToStore(Container::Archery, Container::Weapon);
        AllToStore(Container::OneHand, Container::Weapon);
        AllToStore(Container::TwoHand, Container::Weapon);
        AllToStore(Container::Staff, Container::Weapon);

        StoreToStore(ItemCategory::Archery, Container::Weapon, Container::Archery);
        StoreToStore(ItemCategory::OneHand, Container::Weapon, Container::OneHand);
        StoreToStore(ItemCategory::Staff, Container::Weapon, Container::Staff);
        StoreToStore(ItemCategory::TwoHand, Container::Weapon, Container::TwoHand);

        if (auto source = sourceHandle.get())
            OpenStorageMenu(Container::Weapon, source.get(), [this](RE::TESObjectREFR* a_source) {
                AllToStore(Container::Archery, Container::Weapon);
                AllToStore(Container::OneHand, Container::Weapon);
                AllToStore(Container::TwoHand, Container::Weapon);
                AllToStore(Container::Staff, Container::Weapon);
                WeaponMenu(a_source);
            });
    }});

    options.push_back({"Exit", [this, sourceHandle] {
        if (auto source = sourceHandle.get()) CloseStoreObject(source.get());
    }});

    const auto cancelIndex = static_cast<std::int32_t>(options.size() - 1);
    ShowDynamicMessageBox("Weapon Vault", std::move(options), cancelIndex);
}

void Stores::WeaponArcheryMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowWeaponArcheryMenu(a_openSource); });
}

void Stores::ShowWeaponArcheryMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Archery, Container::Weapon, Container::Archery);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Archery, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Archery, Container::Weapon);
    });
}

void Stores::WeaponOneHandMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowWeaponOneHandMenu(a_openSource); });
}

void Stores::ShowWeaponOneHandMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::OneHand, Container::Weapon, Container::OneHand);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::OneHand, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::OneHand, Container::Weapon);
    });
}

void Stores::WeaponTwoHandMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowWeaponTwoHandMenu(a_openSource); });
}

void Stores::ShowWeaponTwoHandMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::TwoHand, Container::Weapon, Container::TwoHand);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::TwoHand, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::TwoHand, Container::Weapon);
    });
}

void Stores::WeaponStaffMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowWeaponStaffMenu(a_openSource); });
}

void Stores::ShowWeaponStaffMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Staff, Container::Weapon, Container::Staff);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Staff, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Staff, Container::Weapon);
    });
}

//===================================================
// *Misc Menus
//===================================================
void Stores::GemstoneMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowGemstoneMenu(a_openSource); });
}

void Stores::ShowGemstoneMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    StoreToStore(ItemCategory::Gemstone, Container::Smithing, Container::Gemstone);

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Gemstone, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            AllToStore(Container::Gemstone, Container::Smithing);
    });
}

void Stores::TreasureMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowTreasureMenu(a_openSource); });
}

void Stores::ShowTreasureMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    // StoreToStore(ItemCategory::Gemstone, Container::Smith, Container::Gemstone;

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Treasure, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            // AllToStore(Container::Gemstone, Container::Smith);
    });
}

void Stores::StolenMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowStolenMenu(a_openSource); });
}

void Stores::ShowStolenMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    // StoreToStore(ItemCategory::Gemstone, Container::Smith, Container::Gemstone;

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Stolen, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            // AllToStore(Container::Gemstone, Container::Smith);
    });
}

void Stores::FollowerMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    OpenStoreObject(a_source, [this](RE::TESObjectREFR* a_openSource) { ShowFollowerMenu(a_openSource); });
}

void Stores::ShowFollowerMenu(RE::TESObjectREFR* a_source) {
    if (!a_source) return;
    const auto sourceHandle = a_source->GetHandle();

    // Transfer Items
    // StoreToStore(ItemCategory::Gemstone, Container::Smith, Container::Gemstone;

    // Open the container
    if (auto source = sourceHandle.get())
        OpenStorageMenu(Container::Follower, source.get(), [this](RE::TESObjectREFR* a_source) {
            if (a_source) CloseStoreObject(a_source);
            // AllToStore(Container::Gemstone, Container::Smith);
    });
}
