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
	int GSC_AssignKeyCode{ RE::BSKeyboardDevice::Key::kUp };
	bool GSC_GlobalRedirects{ false };

	// Processing
	void LoadINI();
	void SaveINI();

	void ProcessOverrides();
	[[nodiscard]] std::optional<CategoryMask> FindOverride(const RE::TESBoundObject* a_item) const;

	static Settings* GetSingleton();
private:
	std::unordered_map<RE::FormID, CategoryMask> overrides_;

    // cannot use auto for class member declaration
	static constexpr const wchar_t* setting_path = L"Data/SKSE/Plugins/StoreBoundSettings.ini";
	static constexpr const wchar_t* override_path = L"Data/SKSE/Plugins/StoreBoundOverrides.ini";

	template <class Func>
	void ForEachINIOption(Settings& settings, Func&& option);

	// Value Setter/Getter
	template <class T>
	void set_value(CSimpleIniA& a_ini, const T& a_value, const char* a_section, const char* a_key);

	template <class T>
	void get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key);
};
