#include "AVUI.h"
#include "Translation.h"
#include "ItemClass.h"

using AVTranslation::Translate;

namespace AVUI {
	void Register() {
		if (!SKSEMenuFramework::IsInstalled()) return;

		// Render Menu
		SKSEMenuFramework::SetSection(Translate("Settings.ModName"));
		SKSEMenuFramework::AddSectionItem(Translate("Options"), RenderOptions);
		SKSEMenuFramework::AddSectionItem(Translate("Override"), RenderOverrides);
	}

	void __stdcall RenderOptions() {
		Settings* Settings = Settings::GetSingleton();
		bool changeOption = false;

		// Hotkey Options
		Text(Translate("Options.Hotkey"));
		SameLine();
		int &hotkey = Settings->GSC_AssignKeyCode;

		// Show the button with current hotkey name
		if (Button(waitKey ? Translate("Options.WaitKey") : GetKeyName((ImGuiKey)HelperFunctions::IDCodeToImGuiKey(hotkey)))) {
			waitKey = true;
			hotkey = -1;
		}

		// If waiting, check for input
		if (waitKey) {
			for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++) {
				if (IsKeyPressed((ImGuiKey)key)) {
					hotkey = HelperFunctions::ImGuiKeyToIDCode((ImGuiKey)key);
					waitKey = false;
					changeOption = true;
					break;
				}
			}
		}

		// Show a Clear Button
		if (hotkey >= 0) {
			SameLine();
			if (Button(Translate("Options.Reset"))) {
				hotkey = -1;
				changeOption = true;
			}
		}
		if (Checkbox(Translate("Options.Redirect"), &Settings->GSC_GlobalRedirects))
			changeOption = true;		
		if (Checkbox(Translate("Options.CraftLoan"), &Settings->GSC_CraftingLoan))
			changeOption = true;
		if (Checkbox(Translate("Options.OnlyOwn"), &Settings->GSC_AssignOnlyOwn))
			changeOption = true;

		if (changeOption)
			Settings->SaveINI();
	}

	void __stdcall RenderOverrides() {
		auto& editor = overrideEditor;

		if (CollapsingHeader(Translate("Override.Lookup"), ImGuiTreeNodeFlags_DefaultOpen)) {
			// Lookup the form ID of the item
			Text(Translate("Override.FormID"));
			InputText("##FormID", editor.formIDText.data(), editor.formIDText.size(), ImGuiInputTextFlags_CharsHexadecimal);
			SameLine();
			if (Button(Translate("Override.ButtonLookup"))) LookupOverrideItem();

			// Display the record name
			Text(Translate("Override.FormID"));
			InputText("##Object", editor.itemName.data(), editor.itemName.size(), ImGuiInputTextFlags_ReadOnly);

			// If record isnt found
			if (!editor.error.empty()) Text("%s", editor.error.c_str());

			if (editor.selectedItem) {
				const auto selected = std::ranges::find_if(categoryNames, [&editor](const auto& entry) {
					return HasCategory(editor.selectedCategories, entry.second);
				});
				const auto preview = selected != categoryNames.end() ? selected->first : std::string_view{ "Select category" };

				Text(Translate("Override.Categories"));
				if (BeginCombo("##CategorySelection", preview.data())) {
					for (const auto& [name, category] : categoryNames) {
						const bool isSelected = HasCategory(editor.selectedCategories, category);
						if (Selectable(name.data(), isSelected))
							editor.selectedCategories = ToMask(category);
					}
					EndCombo();
				}
			}

			if (editor.selectedFormID != 0 && Button(Translate("Override.ButtonSave"))) SaveOverrideChanges();
		}

		if (CollapsingHeader(Translate("Override.UserOverrides"), ImGuiTreeNodeFlags_DefaultOpen)) {
			auto* settings = Settings::GetSingleton();
			std::optional<RE::FormID> pendingRemoval;

			if (BeginTable("UserOverrides", 4)) {
				TableSetupColumn(Translate("Override.FormID"));
				TableSetupColumn(Translate("Override.Record"));
				TableSetupColumn(Translate("Override.Categories"));
				TableSetupColumn("");
				TableHeadersRow();

				for (const auto& [formID, categories] : settings->GetUserOverrides()) {
					TableNextRow();
					PushID(static_cast<int>(formID));

					TableNextColumn();
					Text("0x%08X", formID);

					TableNextColumn();
					const auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
					Text("%s", item && item->GetName() ? item->GetName() : "<Missing record>");

					TableNextColumn();
					const auto categoryText = FormatCategories(categories);
					Text("%s", categoryText.c_str());

					TableNextColumn();
					if (Button(Translate("Override.ButtonEdit"))) {
						std::snprintf(editor.formIDText.data(), editor.formIDText.size(), "%08X", formID);
						LookupOverrideItem();
					}
					SameLine();
					if (Button(Translate("Override.ButtonRemove")))
						pendingRemoval = formID;

					PopID();
				}
				EndTable();
			}

			if (pendingRemoval && settings->RemoveUserOverride(*pendingRemoval)) {
				settings->SaveUserOverrides();
				if (editor.selectedFormID == *pendingRemoval) {
					editor.selectedFormID = 0;
					editor.selectedItem = nullptr;
					editor.selectedCategories = 0;
					editor.itemName.fill('\0');
				}
			}
		}
	}

	void LookupOverrideItem() {
		auto& editor = overrideEditor;

		editor.selectedItem = nullptr;
		editor.selectedFormID = 0;
		editor.selectedCategories = 0;
		editor.itemName.fill('\0');
		editor.error.clear();

		std::string_view text{ editor.formIDText.data() };

		if (text.starts_with("0x") || text.starts_with("0X"))
			text.remove_prefix(2);

		RE::FormID formID{};
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), formID, 16);

		if (error != std::errc{} || end != text.data() + text.size()) {
			editor.error = "Invalid FormID";
			return;
		}

		// Check overrides first so an existing category selection is restored.
		if (const auto existing = Settings::GetSingleton()->FindOverride(formID))
			editor.selectedCategories = *existing;

		auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
		if (!item) {
			editor.error = "Form is not an item";
			return;
		}

		editor.selectedItem = item;
		editor.selectedFormID = item->GetFormID();
		const std::string_view name{ item->GetName() ? item->GetName() : "" };
		const auto length = (std::min)(name.size(), editor.itemName.size() - 1);
		std::ranges::copy_n(name.begin(), length, editor.itemName.begin());
	}

	void SaveOverrideChanges() {
		auto& editor = overrideEditor;
		auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(editor.selectedFormID);
		if (!item) {
			editor.error = "Look up an item before saving";
			return;
		}

		auto* settings = Settings::GetSingleton();
		settings->SetUserOverride(item, editor.selectedCategories);
		if (settings->SaveUserOverrides())
			editor.error.clear();
		else
			editor.error = "Failed to save user overrides";
	}
}
