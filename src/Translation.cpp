#include "Translation.h"
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>

namespace AVTranslation {
	namespace {
		constexpr const char* translationsFolder = "Data\\SKSE\\Plugins\\ArcaneVault\\ArcaneVaultTranslation.json";
		constexpr const char* defaultTranslation = "missing translation";
		std::map<std::string, std::string> translations;
	}

	void Install() {
		std::ifstream file(translationsFolder);
		if (!file.is_open()) {
			logger::info("No configuration file found at {}", translationsFolder);
			return;
		}

		auto data = nlohmann::json::parse(file);
		logger::trace("reading translation");

		if (!data.is_object()) {
			logger::trace("translation json: {} must be an object", translationsFolder);
			return;
		}

		translations.clear();
		for (auto& [key, value] : data.items()) {
			logger::trace("{} -> {}", key, value.dump());
			translations[key] = value.get<std::string>();
		}
	}

	const char* Translate(const std::string& key) {
		auto it = translations.find(key);
		return it != translations.end() ? it->second.c_str() : defaultTranslation;
	}
}
