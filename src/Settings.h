#pragma once
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include "SimpleIni.h"
#include "ItemClass.h"

// ===========================
// Settings
// ===========================
class Settings {
public:
	// Hotkeys
	int GSC_AssignKeyCode{ -1 };
	bool GSC_GlobalRedirects{ true };
	bool GSC_CraftingLoan{ false };
	bool GSC_AssignOnlyOwn{ true };

	void Init();

	// Processing
	void LoadINI();
	void SaveINI();
	void LoadRedirects();
	void SaveRedirects() const;

	// Overrides
	void ProcessOverrides();
	void ProcessUserOverrides();
	void SetUserOverride(RE::TESBoundObject* a_item, CategoryMask a_categories);
	bool RemoveUserOverride(RE::FormID a_formID);
	bool SaveUserOverrides() const;
	[[nodiscard]] std::optional<CategoryMask> FindOverride(const RE::TESBoundObject* a_item) const;
	[[nodiscard]] std::optional<CategoryMask> FindOverride(RE::FormID a_formID) const;
	[[nodiscard]] const std::unordered_map<RE::FormID, CategoryMask>& GetUserOverrides() const noexcept;

	static Settings* GetSingleton();
private:
	std::unordered_map<RE::FormID, CategoryMask> overrides_;
	std::unordered_map<RE::FormID, CategoryMask> userOverrides_;

    // cannot use auto for class member declaration
	static constexpr const wchar_t* setting_path = L"Data/SKSE/Plugins/ArcaneVault/ArcaneVaultSettings.ini";
	static constexpr const wchar_t* override_path = L"Data/SKSE/Plugins/ArcaneVault/ArcaneVaultOverrides.ini";
	static constexpr const wchar_t* useroverride_path = L"Data/SKSE/Plugins/ArcaneVault/ArcaneVaultUserOverrides.ini";
	static constexpr const wchar_t* redirect_path = L"Data/SKSE/Plugins/ArcaneVault/ArcaneVaultRedirects.ini";

	void ProcessOverrideFile(const wchar_t* a_path , std::unordered_map<RE::FormID, CategoryMask> &a_overrides);

	template <class Func>
	void ForEachINIOption(Settings& settings, Func&& option);

	// Value Setter/Getter
	template <class T>
	void set_value(CSimpleIniA& a_ini, const T& a_value, const char* a_section, const char* a_key);

	template <class T>
	void get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key);
};
