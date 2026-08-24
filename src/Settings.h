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
	int GSC_QuickKeyCode{ -1 };
	bool GSC_GlobalRedirects{ true };
	bool GSC_CraftingLoan{ false };
	bool GSC_AssignOnlyOwn{ true };
	bool GSC_AssignSpell{ true };
	bool GSC_SortSpell{ true };

	bool GSC_OffloadReagent{ true };
	bool GSC_OffloadCatalyst{ true };
	bool GSC_OffloadIngredient{ true };
	bool GSC_OffloadClothing{ false };
	bool GSC_OffloadHeavyArmor{ false };
	bool GSC_OffloadLightArmor{ false };
	bool GSC_OffloadJewelry{ false };
	bool GSC_OffloadBooks{ false };
	bool GSC_OffloadScrolls{ false };
	bool GSC_OffloadSpelltomes{ false };
	bool GSC_OffloadCrafting{ true };
	bool GSC_OffloadConstruction{ true };
	bool GSC_OffloadGemstone{ true };
	bool GSC_OffloadSmelting{ true };
	bool GSC_OffloadTanning{ true };
	bool GSC_OffloadPotion{ false };
	bool GSC_OffloadPoison{ false };
	bool GSC_OffloadCookedFood{ false };
	bool GSC_OffloadRawFood{ true };
	bool GSC_OffloadEmptySoulgem{ false };
	bool GSC_OffloadFilledSoulgem{ false };
	bool GSC_OffloadGrandSoulgem{ true };
	bool GSC_OffloadTreasure{ false };
	bool GSC_OffloadArchery{ false };
	bool GSC_OffloadOneHand{ false };
	bool GSC_OffloadTwoHand{ false };
	bool GSC_OffloadStaffHand{ false };

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
