#include "Settings.h"
#include "Utility.h"
#include "ItemClass.h"

// =============================================================
// Settings
// =============================================================
template <class Func>
void Settings::ForEachINIOption(Settings& settings, Func&& option) {
	// Keycode
	option(settings.GSC_GlobalRedirects, "General", "GlobalRedirect");
	option(settings.GSC_AssignKeyCode, "General", "AssignKeyCode");
}

Settings* Settings::GetSingleton() {
	static Settings singleton;
	return std::addressof(singleton);
}

void Settings::LoadINI() {
	CSimpleIniA iniSettings;
	iniSettings.SetUnicode();
	SI_Error iniError = iniSettings.LoadFile(setting_path);

	ForEachINIOption(*this, [this, &iniSettings](auto& value, const char* section, const char* key) {
		get_value(iniSettings, value, section, key);
	});

	// If not found, create it
	if (iniError < 0)
		iniSettings.SaveFile(setting_path);
}

void Settings::SaveINI() {
	CSimpleIniA iniSettings;
	iniSettings.SetUnicode();
	iniSettings.LoadFile(setting_path);

	ForEachINIOption(*this, [this, &iniSettings](const auto& value, const char* section, const char* key) {
		set_value(iniSettings, value, section, key);
	});

	iniSettings.SaveFile(setting_path);
}

void Settings::ProcessOverrides() {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiKey(true);
	ini.LoadFile(override_path);

	// Get all the overrides
	const CSimpleIniA::TKeyVal* pSection = ini.GetSection("Overrides");
	if (pSection) {
		for (const auto& it : *pSection) {
			// Get the Key and Value
			auto rawKey = it.first.pItem;
			std::string_view key{ rawKey };
			auto rawCategories = it.second;
			if (!rawCategories) continue;

			// Process the key to find the form
			auto separator = key.find('|');
			if (separator == std::string::npos) return;

			// Get the plugin name and form ID text
			const auto pluginName = key.substr(0, separator);
			const auto localIDText = key.substr(separator + 1);

			// Make sure that the FormID is in the correct format
			RE::FormID localID{};
			auto [end, error] = std::from_chars(localIDText.data(), localIDText.data() + localIDText.size(), localID, 16);
			if (error != std::errc{}) return;

			// Get the item
			auto* form = RE::TESDataHandler::GetSingleton()->LookupForm(localID, pluginName);
			auto* item = form ? form->As<RE::TESBoundObject>() : nullptr;
			if (!item) return;

			// Add the item to the override list, and parse its categories
			overrides_.insert_or_assign(item->GetFormID(), ParseCategories(std::string_view{ rawCategories }));
        }
	}

	logger::info("Loaded: {} Overrides", overrides_.size());
}

std::optional<CategoryMask> Settings::FindOverride(const RE::TESBoundObject* a_item) const {
	if (!a_item) return std::nullopt;

	const auto entry = overrides_.find(a_item->GetFormID());
	if (entry == overrides_.end()) return std::nullopt;

	return entry->second;
}

// =============================================================
// INI Functions
// =============================================================
template <class T>
void Settings::set_value(CSimpleIniA& a_ini, const T& a_value, const char* a_section, const char* a_key)
{
	if constexpr (std::is_same_v<T, bool>) {
		a_ini.SetBoolValue(a_section, a_key, a_value);
	} else if constexpr (std::is_same_v<T, std::string>) {
		a_ini.SetValue(a_section, a_key, a_value.c_str());
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		a_ini.SetValue(a_section, a_key, std::format("0x{:X}", a_value).c_str());
	} else {
		a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str());
	}
}

template <class T>
void Settings::get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key)
{
	if (a_ini.KeyExists(a_section, a_key)) {
		if constexpr (std::is_same_v<T, bool>) {
			a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
		} else if constexpr (std::is_same_v<T, std::string>) {
			a_value = a_ini.GetValue(a_section, a_key, a_value.c_str());
		} else if constexpr (std::is_same_v<T, std::uint32_t>) {
			a_value = static_cast<std::uint32_t>(
				std::stoul(a_ini.GetValue(a_section, a_key, std::format("0x{:X}", a_value).c_str()), nullptr, 0)
			);
		} else if constexpr (std::is_integral_v<T>) {
			a_value = static_cast<T>(std::stoi(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str())));
		} else if constexpr (std::is_floating_point_v<T>) {
			a_value = static_cast<T>(std::stof(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str())));
		}
	} else {
		set_value(a_ini, a_value, a_section, a_key);
	}
}
