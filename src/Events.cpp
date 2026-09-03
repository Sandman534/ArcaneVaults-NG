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

class CraftingLoanState {
public:
    static CraftingLoanState* GetSingleton() {
        static CraftingLoanState singleton;
        return &singleton;
    }

    void EnterCrafting(RE::TESObjectREFR* a_furniture) {
        // Set active furniture
        activeFurniture = a_furniture;

        // Check for Craft Loan Setting
        if (!Settings::GetSingleton()->GSC_CraftingLoan) return;

        auto* stores = Stores::GetSingleton();
        auto* utility = Utility::GetSingleton();
        BenchType benchType = utility->GetBenchType(activeFurniture);

        // Begin crafting loan
        switch (benchType) {
        // Alchemy
        case BenchType::Alchemy:
            stores->AllToPlayer(Container::Alchemy);
            break;

        // Smithing
        case BenchType::Smithing:
            stores->AllToPlayer(Container::Smithing);
            stores->StoreToPlayer(ItemCategory::Catalyst, Container::Alchemy);
            break;

        // Cooking
        case BenchType::Cooking:
            stores->AllToPlayer(Container::Food);
            stores->StoreToPlayer(ItemCategory::Reagent, Container::Alchemy);
            break;

        // Enchanting Table
        case BenchType::Enchanting:
            stores->AllToPlayer(Container::Soulgem);
            break;

        // Staff Enchanting
        case BenchType::StaffEnchanting:
            stores->StoreToPlayer(ItemCategory::Catalyst, Container::Alchemy);
            break;
        }
    }

    void ExitCrafting() {        
        // Check for Craft Loan Setting
        if (!Settings::GetSingleton()->GSC_CraftingLoan) return;

        auto* stores = Stores::GetSingleton();
        auto* utility = Utility::GetSingleton();
        BenchType benchType = utility->GetBenchType(activeFurniture);

        // Restore outstanding crafting loan
        switch (benchType) {
        // Alchemy
        case BenchType::Alchemy:
            stores->PlayerToStore(ItemCategory::Alchemy, Container::Alchemy);
            break;

        // Smithing
        case BenchType::Smithing:
            stores->PlayerToStore(ItemCategory::Smithing, Container::Smithing);
            stores->PlayerToStore(ItemCategory::Catalyst, Container::Alchemy);
            break;

        case BenchType::Cooking:
            stores->PlayerToStore(ItemCategory::Food, Container::Food);
            stores->PlayerToStore(ItemCategory::Reagent, Container::Alchemy);
            break;

        // Enchanting Table
        case BenchType::Enchanting:
            stores->PlayerToStore(ItemCategory::Soulgem, Container::Soulgem);
            break;

        // Staff Enchanting
        case BenchType::StaffEnchanting:
            stores->PlayerToStore(ItemCategory::Catalyst, Container::Alchemy);
            break;
        }
    }

private:
    RE::TESObjectREFR* activeFurniture;
};

class FurnitureActivation {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> vtable{ RE::TESFurniture::VTABLE[0] };
        activate_ = vtable.write_vfunc(0x37, ActivateCraftLoan);
        logger::info("Furniture Activation Hook Installed");
    }

private:
    static bool ActivateCraftLoan(RE::TESFurniture* a_baseObject, RE::TESObjectREFR* a_target, RE::TESObjectREFR* a_activator, std::uint8_t a_arg3, RE::TESBoundObject* a_object, std::int32_t a_targetCount) {
        const auto benchType = a_baseObject->workBenchData.benchType.get();
        const bool craftingActivation = a_target && a_activator == RE::PlayerCharacter::GetSingleton() && benchType != RE::TESFurniture::WorkBenchData::BenchType::kNone && Settings::GetSingleton()->GSC_CraftingLoan;

        // Send crafting materials to player
        if (craftingActivation)
            CraftingLoanState::GetSingleton()->EnterCrafting(a_target);

        // Call activate record
        const bool result = activate_(a_baseObject, a_target, a_activator, a_arg3, a_object, a_targetCount);

        return result;
    }

    static inline REL::Relocation<decltype(ActivateCraftLoan)> activate_;
};

class CraftingMenuEventHandler final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static CraftingMenuEventHandler* GetSingleton() {
        static CraftingMenuEventHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override {
        if (!a_event || a_event->menuName != RE::CraftingMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;

        if (!a_event->opening)
            CraftingLoanState::GetSingleton()->ExitCrafting();

        return RE::BSEventNotifyControl::kContinue;
    }

    static void Register() {
        RE::UI::GetSingleton()->AddEventSink(CraftingMenuEventHandler::GetSingleton());
        logger::info("Handler Installed: Crafting Menu");
    }
};

class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
	public:
    static InputHandler* GetSingleton() {
		static InputHandler singleton;
		return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_eventSource) {
		if (a_event) {

			const auto controlMap = RE::ControlMap::GetSingleton();
			const auto playerControls = RE::PlayerControls::GetSingleton();
            auto* settings = Settings::GetSingleton();
            auto* utility = Utility::GetSingleton();
			// If we dont have any of these, return
			if (!controlMap || !playerControls) return RE::BSEventNotifyControl::kContinue;

			// Was a key pressed that matches our menu hotkey
			for (auto event = *a_event; event; event = event->next) {
				if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
					const auto button = static_cast<RE::ButtonEvent*>(event);
					if (!button || (button->IsPressed() && !button->IsDown())) continue;

                    // Get an adjusted scan code
                    auto device = button->device.get();
                    auto scan_code = HelperFunctions::FixCode(device, button->GetIDCode());

					if ((device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kGamepad) && !button->IsUp()) {
						if (scan_code == settings->GSC_AssignKeyCode)
                            utility->ContainerRedirect();
                        
                        if (scan_code == settings->GSC_QuickKeyCode)
                            Stores::GetSingleton()->OffloadItems();
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

namespace AVEvents {
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

        // Furnituer Activation Hook
        FurnitureActivation::Install();

        // Register Events
        ContainerMenuEventHandler::Register();
        CraftingMenuEventHandler::Register();
        InputHandler::Register();
	}
}
