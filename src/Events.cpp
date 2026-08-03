#include "Events.h"
#include "RedirectManager.h"
#include "Utility.h"
#include "Settings.h"
#include "ContainerMenu.h"
#include "Stores.h"
#include "ItemClass.h"

//==============================================
//  Redirects
//==============================================
class RedirectedContainerState {
public:
    static RedirectedContainerState* GetSingleton() {
        static RedirectedContainerState singleton;
        return &singleton;
    }

    void Begin(RE::TESObjectREFR* source, RE::TESObjectREFR* destination) {
        source_ = source->GetHandle().native_handle();
        destination_ = destination->GetHandle().native_handle();
        destinationMenuOpen_ = false;
    }

    void Cancel() {
        CloseSource();
        Reset();
    }

    void OnContainerMenuOpen() {
        if (destination_ != RE::RefHandle{} && RE::ContainerMenu::GetTargetRefHandle() == destination_)
            destinationMenuOpen_ = true;
    }

    void OnContainerMenuClose() {
        if (destinationMenuOpen_) {
            CloseSource();
            Reset();
        }
    }

    void Reset() {
        source_ = {};
        destination_ = {};
        destinationMenuOpen_ = false;
    }

private:
    void CloseSource() {
        if (auto source = RE::TESObjectREFR::LookupByHandle(source_)) {
            RE::BGSOpenCloseForm::SetOpenState(source.get(), false, false);
        }
    }

    RE::RefHandle source_;
    RE::RefHandle destination_;
    bool destinationMenuOpen_{ false };
};

class ContainerMenuCloseCallback {
public:
    static ContainerMenuCloseCallback* GetSingleton() {
        static ContainerMenuCloseCallback singleton;
        return &singleton;
    }

    void Begin(RE::TESObjectREFR* a_container, std::function<void()> a_callback) {
        container_ = a_container ? a_container->GetHandle().native_handle() : RE::RefHandle{};
        callback_ = std::move(a_callback);
        menuOpened_ = false;
    }

    void Cancel() {
        container_ = {};
        callback_ = {};
        menuOpened_ = false;
    }

    void OnContainerMenuOpen() {
        if (container_ != RE::RefHandle{} && RE::ContainerMenu::GetTargetRefHandle() == container_) {
            menuOpened_ = true;
        }
    }

    void OnContainerMenuClose() {
        if (!menuOpened_ || !callback_) return;

        auto callback = std::move(callback_);
        Cancel();
        SKSE::GetTaskInterface()->AddTask(std::move(callback));
    }

private:
    RE::RefHandle container_;
    std::function<void()> callback_;
    bool menuOpened_{ false };
};

//==============================================
// Hooks
//==============================================
class ContainerActivationHook {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> vtable{ RE::TESObjectCONT::VTABLE[0] };
        original_ = vtable.write_vfunc(0x37, Thunk);
        logger::info("Container activation hook installed.");
    }

private:
    static bool Thunk(RE::TESObjectCONT* container, RE::TESObjectREFR* target, RE::TESObjectREFR* activator, std::uint8_t arg3, RE::TESBoundObject* object, std::int32_t targetCount) {
        static thread_local bool redirecting = false;

        if (redirecting || !target || activator != RE::PlayerCharacter::GetSingleton() || target->IsLocked())
            return original_(container, target, activator, arg3, object, targetCount);

        auto* destination = RedirectManager::GetSingleton()->FindDestination(target);
        if (!destination || destination == target || !destination->GetBaseObject())
            return original_(container, target, activator, arg3, object, targetCount);

        // logger::debug("Redirecting activation of {:08X} through destination {:08X}.", target->GetFormID(), destination->GetFormID());
        if (Utility::GetSingleton()->RunContainerAction(destination, target))
            return true;

        return original_(container, target, activator, arg3, object, targetCount);
    }

    static inline REL::Relocation<decltype(Thunk)> original_;
};

class ContainerActivateTextHook {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> vtable{ RE::TESObjectCONT::VTABLE[0] };
        original_ = vtable.write_vfunc(0x4C, Thunk);
        logger::info("Container activation-text hook installed");
    }

private:
    static bool Thunk(RE::TESObjectCONT* a_container, RE::TESObjectREFR* a_activator, RE::BSString& a_text) {
        const auto result = original_(a_container, a_activator, a_text);

        auto* crosshair = RE::CrosshairPickData::GetSingleton();
        auto target = crosshair ? crosshair->target.get() : nullptr;

        if (target) {
            auto* destination = RedirectManager::GetSingleton()->FindDestination(target.get());

            if (destination) {
                const auto name = destination->GetDisplayFullName();
                if (name && name[0] != '\0') {
                    const auto text = std::format("Search\n{}", name);
                    a_text = text.c_str();
                    return true;
                }
            }
        }
        return result;
    }

    static inline REL::Relocation<decltype(Thunk)> original_;
};

class ActivatorActivationHook {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> vtable{ RE::TESObjectACTI::VTABLE[0] };

        original_ = vtable.write_vfunc(0x37, Thunk);
        logger::info("Activator activation hook installed");
    }

private:
    static bool Thunk(RE::TESObjectACTI* baseObject, RE::TESObjectREFR* target, RE::TESObjectREFR* activator, std::uint8_t arg3, RE::TESBoundObject* object, std::int32_t targetCount) {
        if (!target || activator != RE::PlayerCharacter::GetSingleton())
            return original_(baseObject, target, activator, arg3, object, targetCount);

        auto* destination = RedirectManager::GetSingleton()->FindDestination(target);
        if (!destination || destination == target)
            return original_(baseObject, target, activator, arg3, object,targetCount);

        // logger::debug("Redirecting activator {:08X} through {:08X}", target->GetFormID(), destination->GetFormID());
        if (Utility::GetSingleton()->RunContainerAction(destination, target))
            return true;

        return original_(baseObject, target, activator, arg3, object, targetCount);
    }

    static inline REL::Relocation<decltype(Thunk)> original_;
};

class ActivatorActivateTextHook {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> vtable{ RE::TESObjectACTI::VTABLE[0] };
        original_ = vtable.write_vfunc(0x4C, Thunk);
        logger::info("Activator activation-text hook installed");
    }

private:
    static bool Thunk(RE::TESObjectACTI* a_baseobject, RE::TESObjectREFR* a_activator, RE::BSString& a_text) {
        const auto result = original_(a_baseobject, a_activator, a_text);

        auto* crosshair = RE::CrosshairPickData::GetSingleton();
        auto target = crosshair ? crosshair->target.get() : nullptr;

        if (target) {
            auto* destination = RedirectManager::GetSingleton()->FindDestination(target.get());

            if (destination) {
                const char* name = destination->GetDisplayFullName();
                if (name && name[0] != '\0') {
                    const auto newText = std::format("Search\n{}", name);
                    a_text = newText.c_str();
                    return true;
                }
            }
        }
        return result;
    }

    static inline REL::Relocation<decltype(Thunk)> original_;
};

//==============================================
// Event Handlers
//==============================================
class ContainerMenuEventHandler final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static ContainerMenuEventHandler* GetSingleton() {
        static ContainerMenuEventHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        [[maybe_unused]] RE::BSTEventSource<RE::MenuOpenCloseEvent>* eventSource) override {
        if (!event || event->menuName != RE::ContainerMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;

        if (event->opening) {
            RedirectedContainerState::GetSingleton()->OnContainerMenuOpen();
            ContainerMenuCloseCallback::GetSingleton()->OnContainerMenuOpen();
        } else {
            RedirectedContainerState::GetSingleton()->OnContainerMenuClose();
            ContainerMenuCloseCallback::GetSingleton()->OnContainerMenuClose();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

	static void Register() {
        RE::UI* uiManager = RE::UI::GetSingleton();
        uiManager->AddEventSink(ContainerMenuEventHandler::GetSingleton());
		logger::info("Handler Installed: Container Event");
    }
};

class FurnitureEventHandler final : public RE::BSTEventSink<RE::TESFurnitureEvent> {
public:
    static FurnitureEventHandler* GetSingleton() {
        static FurnitureEventHandler singleton;
        return std::addressof(singleton);
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESFurnitureEvent>* a_eventSource) override {
        if (!a_event || !a_event->actor || !a_event->targetFurniture)
            return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_event->actor.get() != player)
            return RE::BSEventNotifyControl::kContinue;

        auto* furniture = a_event->targetFurniture->GetBaseObject()->As<RE::TESFurniture>();
        if (!furniture)
            return RE::BSEventNotifyControl::kContinue;

        const auto benchType = furniture->workBenchData.benchType.get();
        if (benchType == RE::TESFurniture::WorkBenchData::BenchType::kNone)
            return RE::BSEventNotifyControl::kContinue;

        switch (a_event->type.get()) {
        case RE::TESFurnitureEvent::FurnitureEventType::kEnter:
            OnCraftingFurnitureEntered(a_event->targetFurniture.get(), benchType);
            break;

        case RE::TESFurnitureEvent::FurnitureEventType::kExit:
            OnCraftingFurnitureExited(a_event->targetFurniture.get(), benchType);
            break;
        }

        return RE::BSEventNotifyControl::kContinue;
    }

	static void Register() {
        RE::ScriptEventSourceHolder* scriptSourceManager = RE::ScriptEventSourceHolder::GetSingleton();
        scriptSourceManager->AddEventSink(FurnitureEventHandler::GetSingleton());
		logger::info("Handler Installed: Furniture");
    }

private:
    static void OnCraftingFurnitureEntered(RE::TESObjectREFR* a_furniture, RE::TESFurniture::WorkBenchData::BenchType a_benchType) {
        logger::info("Player entered crafting furniture {:08X}, bench type {}", a_furniture->GetFormID(), std::to_underlying(a_benchType));

        // Check for Craft Loan Setting
        if (!Settings::GetSingleton()->GSC_CraftingLoan) return;

        // Begin crafting loan
        switch (a_benchType) {
        // Alchemy
        case RE::TESFurniture::WorkBenchData::BenchType::kAlchemy:
            Stores::GetSingleton()->AllToPlayer(Container::Alchemy);
            break;

        // Smithing
        case RE::TESFurniture::WorkBenchData::BenchType::kCreateObject:
        case RE::TESFurniture::WorkBenchData::BenchType::kSmithingArmor:    
        case RE::TESFurniture::WorkBenchData::BenchType::kSmithingWeapon:
            Stores::GetSingleton()->AllToPlayer(Container::Smithing);
            break;

        // Enchanting Table
        case RE::TESFurniture::WorkBenchData::BenchType::kEnchanting:
            Stores::GetSingleton()->AllToPlayer(Container::Soulgem);
            break;
        }
    }

    static void OnCraftingFurnitureExited(RE::TESObjectREFR* a_furniture, RE::TESFurniture::WorkBenchData::BenchType a_benchType) {
        logger::info("Player exited crafting furniture {:08X}, bench type {}", a_furniture->GetFormID(), std::to_underlying(a_benchType));
        
        // Check for Craft Loan Setting
        if (!Settings::GetSingleton()->GSC_CraftingLoan) return;

        // Restore outstanding crafting loan
        switch (a_benchType) {
        // Alchemy
        case RE::TESFurniture::WorkBenchData::BenchType::kAlchemy:
            Stores::GetSingleton()->PlayerToStore(ItemCategory::Ingredient, Container::Alchemy);
            Stores::GetSingleton()->PlayerToStore(ItemCategory::Reagent, Container::Alchemy);
            break;

        // Smithing
        case RE::TESFurniture::WorkBenchData::BenchType::kCreateObject:
        case RE::TESFurniture::WorkBenchData::BenchType::kSmithingArmor:    
        case RE::TESFurniture::WorkBenchData::BenchType::kSmithingWeapon:
            Stores::GetSingleton()->PlayerToStore(ItemCategory::Smithing, Container::Smithing);
            break;

        // Enchanting Table
        case RE::TESFurniture::WorkBenchData::BenchType::kEnchanting:
            Stores::GetSingleton()->PlayerToStore(ItemCategory::Soulgem, Container::Soulgem);
            break;
        }
    }
};

namespace
{
    bool PlayerOwnsCurrentCell(const RE::PlayerCharacter* a_player)
    {
        if (!a_player) return false;

        auto* cell = a_player->GetParentCell();
        if (!cell) return false;

        if (const auto* actorOwner = cell->GetActorOwner())
            return actorOwner == a_player->GetActorBase();

        if (const auto* factionOwner = cell->GetFactionOwner())
            return a_player->IsInFaction(factionOwner);

        return false;
    }
}

class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
	public:
    static InputHandler* GetSingleton() {
		static InputHandler singleton;
		return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_eventSource) {
		if (a_event) {

			const auto controlMap = RE::ControlMap::GetSingleton();
			const auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			const auto playerControls = RE::PlayerControls::GetSingleton();
            auto settings = Settings::GetSingleton();
			// If we dont have any of these, return
			if (!controlMap || !playerCharacter || !playerControls) return RE::BSEventNotifyControl::kContinue;

			// Was a key pressed that matches our menu hotkey
			for (auto event = *a_event; event; event = event->next) {
				if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
					const auto button = static_cast<RE::ButtonEvent*>(event);
					if (!button || (button->IsPressed() && !button->IsDown())) continue;

					auto device = button->device.get();
					auto scan_code = button->GetIDCode();

					if ((device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kGamepad) && !button->IsUp()) {
                        auto assignKeyCode = settings->GSC_AssignKeyCode;
						if (assignKeyCode >= 0 && scan_code == static_cast<decltype(scan_code)>(assignKeyCode)) {
                            auto target = RE::CrosshairPickData::GetSingleton()->target.get();
                            if (!target) continue;
                            
                            // Check player cell
                            const bool ownsCell = PlayerOwnsCurrentCell(playerCharacter);

                            // Container
                            if (auto container = target->GetBaseObject()->As<RE::TESObjectCONT>()) {
                                const bool nonRespawning = !container->data.flags.all(RE::CONT_DATA::Flag::kRespawn);
                                const bool canAssign = !settings->GSC_AssignOnlyOwn || ownsCell || nonRespawning;

                                if (canAssign)
                                    Stores::GetSingleton()->VaultAssignment(target);
                            }
                            
                            // Activate linked to container
                            else if (target->GetBaseObject()->As<RE::TESObjectACTI>()) {
                                if (auto* ref = target.get()->GetLinkedRef(nullptr)) {
                                    if (auto container = ref->GetBaseObject()->As<RE::TESObjectCONT>()) {
                                        const bool nonRespawning = !container->data.flags.all(RE::CONT_DATA::Flag::kRespawn);
                                        const bool canAssign = !settings->GSC_AssignOnlyOwn || ownsCell || nonRespawning;

                                        if (canAssign)
                                            Stores::GetSingleton()->VaultAssignment(target);
                                    }
                                }
                            }

						}
					}
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	static void Register() {
        RE::BSInputDeviceManager* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
        inputDeviceManager->AddEventSink(InputHandler::GetSingleton());
		logger::info("Handler Installed: Input");
    }
};

namespace Events {
	void RunAfterContainerMenuCloses(RE::TESObjectREFR* a_container, std::function<void()> a_callback) {
        ContainerMenuCloseCallback::GetSingleton()->Begin(a_container, std::move(a_callback));
    }

	void CancelContainerMenuCloseCallback() {
        ContainerMenuCloseCallback::GetSingleton()->Cancel();
    }

	void Init(void) {
        // Container Hooks
        ContainerActivationHook::Install();
        ContainerActivateTextHook::Install();

        // Activator Hooks
        ActivatorActivationHook::Install();
        ActivatorActivateTextHook::Install();

        // Register Events
        ContainerMenuEventHandler::Register();
        FurnitureEventHandler::Register();
        InputHandler::Register();
	}
}
