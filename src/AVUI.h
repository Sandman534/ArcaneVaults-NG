#pragma once
#include "Settings.h"

using namespace ImGuiMCP;

namespace AVUI {
    struct OverrideEditorState
    {
        std::array<char, 11> formIDText{};
        RE::FormID selectedFormID{ 0 };
        RE::TESBoundObject* selectedItem{ nullptr };
        CategoryMask selectedCategories{ 0 };
        std::array<char, 256> itemName{};
        std::string error;
    };

    inline OverrideEditorState overrideEditor;

    void Register();
    
    // Static Variables
	inline bool waitKey = false;

    // Render Functions
    void __stdcall RenderOptions();
    void __stdcall RenderTransfer();
	void __stdcall RenderOverrides();

    // Additional Functions
	void LookupOverrideItem();
    void SaveOverrideChanges();
};
