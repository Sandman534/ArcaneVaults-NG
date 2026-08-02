#include "Events.h"
#include "RedirectManager.h"
#include "Utility.h"
#include "Settings.h"
#include "ContainerMenu.h"

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
        if (destination_ != RE::RefHandle{} && RE::ContainerMenu::GetTargetRefHandle() == destination_) {
            destinationMenuOpen_ = true;
        }
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

class ContainerMenuEventHandler final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static ContainerMenuEventHandler* GetSingleton() {
        static ContainerMenuEventHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        [[maybe_unused]] RE::BSTEventSource<RE::MenuOpenCloseEvent>* eventSource) override {
        if (!event || event->menuName != RE::ContainerMenu::MENU_NAME) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (event->opening) {
            RedirectedContainerState::GetSingleton()->OnContainerMenuOpen();
            ContainerMenuCloseCallback::GetSingleton()->OnContainerMenuOpen();
        } else {
            RedirectedContainerState::GetSingleton()->OnContainerMenuClose();
            ContainerMenuCloseCallback::GetSingleton()->OnContainerMenuClose();
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
	public:
    static InputHandler* GetSingleton() {
		static InputHandler singleton;
		return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) {
		if (a_event) {

			const auto controlMap = RE::ControlMap::GetSingleton();
			const auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			const auto playerControls = RE::PlayerControls::GetSingleton();

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
						auto durability = Settings::GetSingleton();
						if (durability && scan_code == Settings::GetSingleton()->GSC_AssignKeyCode) {
                            auto target = RE::CrosshairPickData::GetSingleton()->target.get();
                            
                            if (!target) continue;

                            // Container
                            if (target->GetBaseObject()->As<RE::TESObjectCONT>())
                                Stores::GetSingleton()->VaultAssignment(target);
                            
                            // Activate linked to container
                            else if (target->GetBaseObject()->As<RE::TESObjectACTI>()) {
                                auto* ref = target.get()->GetLinkedRef(nullptr);
                                if (ref && ref->GetBaseObject()->As<RE::TESObjectCONT>())
                                    Stores::GetSingleton()->VaultAssignment(target);
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
        // Container and Activator Hooks
        ContainerActivationHook::Install();
        ContainerActivateTextHook::Install();
        ActivatorActivationHook::Install();
        ActivatorActivateTextHook::Install();

        // Register the container menu
        RE::UI::GetSingleton()->AddEventSink(ContainerMenuEventHandler::GetSingleton());
        InputHandler::Register();
	}
}
