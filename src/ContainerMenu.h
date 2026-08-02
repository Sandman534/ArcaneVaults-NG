#pragma once

#include "Utility.h"

class ContainerSelectionState {
public:
    using ContainerChoice = Utility::ContainerChoice;
    
    static ContainerSelectionState* GetSingleton();

    void Begin(RE::TESObjectREFRPtr source, std::vector<ContainerChoice> choices);
    [[nodiscard]] bool Complete(std::int32_t selectedIndex);
    void Reset();

    [[nodiscard]] const auto& GetChoices() const {
        return choices_;
    }

private:
    RE::TESObjectREFRPtr source_;
    std::vector<ContainerChoice> choices_;
};

class ContainerListMenu final : public RE::IMenu {
public:
    static constexpr std::string_view MENU_NAME = "GSCContainerListMenu";

    ContainerListMenu();
    void PostCreate() override;
    static RE::IMenu* Create();

private:
    void InitializeList();
    static bool IsUsingGamepad();

    RE::GPtr<RE::GFxFunctionHandler> exitHandler_;
};

namespace Menus {
    void Init();
}
