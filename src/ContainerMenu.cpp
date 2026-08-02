#include "ContainerMenu.h"
#include "RedirectManager.h"

namespace {
    class ExitMenuHandler final : public RE::GFxFunctionHandler {
    public:
        void Call(Params& params) override {
            auto* movie = params.movie;
            std::int32_t selectedIndex = -1;

            if (movie) {
                RE::GFxValue index;
                // Match SkyUILib's getActiveMenuIndex() implementation.
                if (movie->GetVariable(&index, "_root.listDialog.menuList.listState.activeEntry.itemIndex") && index.IsNumber()) {
                    selectedIndex = static_cast<std::int32_t>(index.GetNumber());
                }
            }

            // if (!ContainerSelectionState::GetSingleton()->Complete(selectedIndex)) {
            //     logger::warn("Container selection was not mapped (selected index: {})", selectedIndex);
            // }

            if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                queue->AddMessage(ContainerListMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
            }
        }
    };
}

ContainerSelectionState* ContainerSelectionState::GetSingleton() {
    static ContainerSelectionState singleton;
    return std::addressof(singleton);
}

void ContainerSelectionState::Begin(RE::TESObjectREFRPtr source, std::vector<ContainerChoice> choices) {
    source_ = std::move(source);
    choices_ = std::move(choices);
}

bool ContainerSelectionState::Complete(std::int32_t selectedIndex) {
    if (!source_ || selectedIndex < 0 || static_cast<std::size_t>(selectedIndex) >= choices_.size()) {
        Reset();
        return false;
    }

    const auto sourceID = source_->GetFormID();
    const auto& choice = choices_[selectedIndex];
    auto* destination = RE::TESForm::LookupByID<RE::TESObjectREFR>(choice.formID);
    bool mapped = false;

    if (destination && destination != source_.get()) {
        mapped = RedirectManager::GetSingleton()->Add(source_.get(), destination);
        // if (mapped)
        //     logger::info("Mapped target container {:08X} to '{}' ({:08X})", sourceID, choice.name, destination->GetFormID());
    }

    Reset();
    return mapped;
}

void ContainerSelectionState::Reset() {
    source_.reset();
    choices_.clear();
}

//==================================
// ContainerListMenu
//=================================
ContainerListMenu::ContainerListMenu() {
    menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kUsesCursor, RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kModal);
    inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;

    auto* scaleform = RE::BSScaleformManager::GetSingleton();
    if (!scaleform || !scaleform->LoadMovie(this, uiMovie, "uilib/uilib_1_listmenu"))
        logger::error("Failed to load SkyUILib list menu");
}

void ContainerListMenu::PostCreate() {
    IMenu::PostCreate();
    InitializeList();
}

RE::IMenu* ContainerListMenu::Create() {
    return new ContainerListMenu();
}

void ContainerListMenu::InitializeList() {
    if (!uiMovie) return;

    const auto& choices = ContainerSelectionState::GetSingleton()->GetChoices();

    // SkyUILib: listDialog.setPlatform(Game.UsingGamepad())
    std::array<RE::GFxValue, 1> platformArgs;
    platformArgs[0].SetNumber(IsUsingGamepad() ? 1.0 : 0.0);

    const auto platformSize = static_cast<std::int32_t>(platformArgs.size());
    uiMovie->Invoke("_root.listDialog.setPlatform", nullptr, platformArgs.data(), platformSize);

    // SkyUILib: listDialog.initListData(options)
    // This mirrors Papyrus UI.InvokeStringA: every option is a separate
    // ActionScript argument. Passing a GFx array as one argument causes the
    // list menu to coerce it to one comma-separated string.
    std::vector<RE::GFxValue> optionArgs(choices.size());
    for (std::size_t i = 0; i < choices.size(); ++i) {
        optionArgs[i].SetString(choices[i].name.c_str());
    }

    const auto listdataSize = static_cast<std::int32_t>(optionArgs.size());
    uiMovie->Invoke("_root.listDialog.initListData", nullptr, optionArgs.data(), listdataSize);

    // SkyUILib: listDialog.initListParams(title, start, default)
    std::array<RE::GFxValue, 3> parameterArgs;
    parameterArgs[0].SetString("Select destination");
    parameterArgs[1].SetNumber(0);
    parameterArgs[2].SetNumber(0);

    const auto parameterSize = static_cast<std::int32_t>(parameterArgs.size());
    uiMovie->Invoke("_root.listDialog.initListParams", nullptr, parameterArgs.data(), parameterSize);

    // SkyUILib normally runs inside the engine's "CustomMenu" and its
    // exitMenu() function closes that hard-coded name. This menu has its own
    // registration name, so replace the callback with one that closes us.
    RE::GFxValue listDialog;
    RE::GFxValue exitFunction;
    if (uiMovie->GetVariable(&listDialog, "_root.listDialog") && listDialog.IsObject()) {
        exitHandler_ = RE::GPtr<RE::GFxFunctionHandler>{ new ExitMenuHandler() };
        uiMovie->CreateFunction(&exitFunction, exitHandler_.get());
        listDialog.SetMember("exitMenu", exitFunction);
    } else {
        logger::error("Failed to install SkyUILib exit callback");
    }
}

bool ContainerListMenu::IsUsingGamepad() {
    auto* manager = RE::BSInputDeviceManager::GetSingleton();
    return manager && manager->IsGamepadEnabled();
}

namespace Menus {
	void Init(void) {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            logger::error("UI singleton unavailable");
            return;
        }

        ui->Register(ContainerListMenu::MENU_NAME, ContainerListMenu::Create);
	}
}
