#include "Settings.h"
#include "Utility.h"
#include "ItemClass.h"
#include "RedirectManager.h"

namespace
{
	constexpr std::string_view redirectSection = "Redirects";

	std::optional<std::pair<std::string_view, RE::FormID>> ParseFormIdentifier(std::string_view a_identifier)
	{
		const auto separator = a_identifier.find('|');
		if (separator == std::string_view::npos || separator == 0 || separator + 1 == a_identifier.size())
			return std::nullopt;

		const auto plugin = a_identifier.substr(0, separator);
		auto formIDText = a_identifier.substr(separator + 1);
		if (formIDText.starts_with("0x") || formIDText.starts_with("0X"))
			formIDText.remove_prefix(2);

		RE::FormID localID{};
		const auto [end, error] = std::from_chars(
			formIDText.data(), formIDText.data() + formIDText.size(), localID, 16);
		if (error != std::errc{} || end != formIDText.data() + formIDText.size())
			return std::nullopt;

		return std::pair{ plugin, localID };
	}

	std::optional<std::string> MakeFormIdentifier(const RE::TESForm* a_form)
	{
		if (!a_form || a_form->IsDynamicForm())
			return std::nullopt;

		const auto* file = a_form->GetFile(0);
		if (!file || file->GetFilename().empty())
			return std::nullopt;

		return std::format("{}|0x{:X}", file->GetFilename(), a_form->GetLocalFormID());
	}

}

// =============================================================
// Settings
// =============================================================
template <class Func>
void Settings::ForEachINIOption(Settings& settings, Func&& option) {
	// Options
	option(settings.GSC_GlobalRedirects, "General", "GlobalRedirect");
	option(settings.GSC_AssignKeyCode, "General", "AssignKeyCode");
	option(settings.GSC_CraftingLoan, "General", "CraftingLoan");
	option(settings.GSC_AssignOnlyOwn, "General", "AssignOnlyOwn");
}

Settings* Settings::GetSingleton() {
	static Settings singleton;
	return std::addressof(singleton);
}

void Settings::Init() {
	LoadINI();
	ProcessOverrides();
	ProcessUserOverrides();
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

void Settings::SaveRedirects() const {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(redirect_path);
	ini.Delete(redirectSection.data(), nullptr);

	std::size_t saved{};
	for (const auto& [sourceID, redirect] : RedirectManager::GetSingleton()->GetRedirects()) {
		const auto* source = RE::TESForm::LookupByID<RE::TESObjectREFR>(sourceID);
		const auto* destination = RE::TESForm::LookupByID<RE::TESObjectREFR>(redirect.destinationID);
		const auto sourceIdentifier = MakeFormIdentifier(source);
		const auto destinationIdentifier = MakeFormIdentifier(destination);
		if (!sourceIdentifier || !destinationIdentifier) {
			logger::warn("Could not save global redirect {:08X} -> {:08X}; dynamic or unresolved forms are not stable across games",
				sourceID, redirect.destinationID);
			continue;
		}

		ini.SetValue(redirectSection.data(), sourceIdentifier->c_str(), destinationIdentifier->c_str());
		++saved;
	}

	if (ini.SaveFile(redirect_path) < 0) {
		logger::error("Failed to save global redirects INI");
		return;
	}

	logger::info("Saved {} global redirects", saved);
}

void Settings::LoadRedirects() {
	auto* manager = RedirectManager::GetSingleton();
	manager->Clear();

	CSimpleIniA ini;
	ini.SetUnicode();
	if (ini.LoadFile(redirect_path) < 0) {
		logger::info("No global redirects INI found");
		return;
	}

	const auto* entries = ini.GetSection(redirectSection.data());
	if (!entries) {
		logger::info("Loaded 0 global redirects");
		return;
	}

	auto* dataHandler = RE::TESDataHandler::GetSingleton();
	std::size_t loaded{};
	for (const auto& entry : *entries) {
		const std::string_view sourceText{ entry.first.pItem };
		const std::string_view destinationText{ entry.second ? entry.second : "" };
		const auto sourceIdentifier = ParseFormIdentifier(sourceText);
		const auto destinationIdentifier = ParseFormIdentifier(destinationText);
		if (!sourceIdentifier || !destinationIdentifier) {
			logger::warn("Ignoring malformed global redirect '{}' -> '{}'", sourceText, destinationText);
			continue;
		}

		const auto& [sourcePlugin, sourceLocalID] = *sourceIdentifier;
		const auto& [destinationPlugin, destinationLocalID] = *destinationIdentifier;
		auto* sourceForm = dataHandler->LookupForm(sourceLocalID, sourcePlugin);
		auto* destinationForm = dataHandler->LookupForm(destinationLocalID, destinationPlugin);
		auto* source = sourceForm ? sourceForm->As<RE::TESObjectREFR>() : nullptr;
		auto* destination = destinationForm ? destinationForm->As<RE::TESObjectREFR>() : nullptr;
		if (!source || !destination) {
			logger::warn("Could not resolve global redirect '{}' -> '{}'", sourceText, destinationText);
			continue;
		}

		if (manager->Add(source, destination))
			++loaded;
	}

	logger::info("Loaded {} global redirects", loaded);
}

void Settings::ProcessOverrides() {
	ProcessOverrideFile(override_path, overrides_);
}

void Settings::ProcessUserOverrides() {
	ProcessOverrideFile(useroverride_path, userOverrides_);
}

void Settings::ProcessOverrideFile(const wchar_t* a_path , std::unordered_map<RE::FormID, CategoryMask> &a_overrides) {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiKey(true);
	ini.LoadFile(a_path);

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
			auto pluginName = key.substr(0, separator);
			auto localIDText = key.substr(separator + 1);

			if (localIDText.starts_with("0x") || localIDText.starts_with("0X"))
				localIDText.remove_prefix(2);

			// Make sure that the FormID is in the correct format
			RE::FormID localID{ };
			auto [end, error] = std::from_chars(localIDText.data(), localIDText.data() + localIDText.size(), localID, 16);
			if (error != std::errc{}) return;

			// Get the item
			auto* form = RE::TESDataHandler::GetSingleton()->LookupForm(localID, pluginName);
			auto* item = form ? form->As<RE::TESBoundObject>() : nullptr;
			if (!item) return;

			// Add the item to the override list, and parse its categories
			a_overrides.insert_or_assign(item->GetFormID(), ParseCategories(std::string_view{ rawCategories }));
        }
	}

	logger::info("Loaded: {} Overrides", a_overrides.size());
}

void Settings::SetUserOverride(RE::TESBoundObject* a_item, CategoryMask a_categories) {
	if (!a_item) return;
	userOverrides_.insert_or_assign(a_item->GetFormID(), a_categories);
}

bool Settings::RemoveUserOverride(RE::FormID a_formID) {
	return userOverrides_.erase(a_formID) != 0;
}

bool Settings::SaveUserOverrides() const {
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(useroverride_path);
	ini.Delete("Overrides", nullptr);

	std::size_t saved{};
	for (const auto& [formID, categories] : userOverrides_) {
		const auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
		const auto identifier = MakeFormIdentifier(item);
		if (!identifier) {
			logger::warn("Could not save user override {:08X}; dynamic or unresolved forms are not stable across loads", formID);
			continue;
		}

		const auto categoryText = FormatCategories(categories);
		ini.SetValue("Overrides", identifier->c_str(), categoryText.c_str());
		++saved;
	}

	if (ini.SaveFile(useroverride_path) < 0) {
		logger::error("Failed to save user overrides INI");
		return false;
	}

	logger::info("Saved {} user overrides", saved);
	return true;
}

std::optional<CategoryMask> Settings::FindOverride(const RE::TESBoundObject* a_item) const {
	if (!a_item) return std::nullopt;
	return FindOverride(a_item->GetFormID());
}

std::optional<CategoryMask> Settings::FindOverride(RE::FormID a_formID) const {
	if (const auto userEntry = userOverrides_.find(a_formID); userEntry != userOverrides_.end())
		return userEntry->second;

	const auto entry = overrides_.find(a_formID);
	if (entry == overrides_.end()) return std::nullopt;

	return entry->second;
}

const std::unordered_map<RE::FormID, CategoryMask>& Settings::GetUserOverrides() const noexcept {
	return userOverrides_;
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
