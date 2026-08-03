#include "Serialization.h"
#include "RedirectManager.h"
#include "Settings.h"

namespace Serialization
{
    void SaveCallback(SKSE::SerializationInterface* a_skse) {
        if (!a_skse) return;

        // Global redirects are shared through the INI instead of stored per save.
        if (Settings::GetSingleton()->GSC_GlobalRedirects) {
            Settings::GetSingleton()->SaveRedirects();
            return;
        }

        if (!a_skse->OpenRecord(SerializationType, SerializationVersion)) return;

        const auto& redirects = RedirectManager::GetSingleton()->GetRedirects();
        const auto count = static_cast<std::uint32_t>(redirects.size());
        if (!a_skse->WriteRecordData(&count, sizeof(count))) return;

        for (const auto& [sourceID, redirect] : redirects) {
            if (!a_skse->WriteRecordData(&sourceID, sizeof(sourceID)) ||
                !a_skse->WriteRecordData(&redirect.destinationID, sizeof(redirect.destinationID))) {
                logger::error("Failed to serialize redirect {:08X} -> {:08X}", sourceID, redirect.destinationID);
                return;
            }
        }

        logger::info("Saved {} redirects", count);
    }

    void LoadCallback(SKSE::SerializationInterface* a_skse) {
        if (!a_skse) return;

        // Global redirects are shared through the INI instead of stored per save.
        if (Settings::GetSingleton()->GSC_GlobalRedirects) {
            Settings::GetSingleton()->LoadRedirects();
            return;
        }

        auto* manager = RedirectManager::GetSingleton();
        manager->Clear();

        std::uint32_t type{};
        std::uint32_t version{};
        std::uint32_t length{};
        std::uint32_t restored{};

        while (a_skse->GetNextRecordInfo(type, version, length)) {
            if (type != SerializationType) {
                logger::warn("Ignoring unknown serialization record {:08X}", type);
                continue;
            }
            if (version != SerializationVersion) {
                logger::warn("Unsupported redirect serialization version {}", version);
                continue;
            }

            std::uint32_t count{};
            if (length < sizeof(count) || a_skse->ReadRecordData(count) != sizeof(count)) {
                logger::error("Redirect serialization record is truncated");
                continue;
            }

            constexpr std::uint32_t entrySize = sizeof(RE::FormID) * 2;
            const auto availableEntries = static_cast<std::uint32_t>((length - sizeof(count)) / entrySize);
            if (count > availableEntries) {
                logger::error("Redirect count {} exceeds record capacity {}", count, availableEntries);
                count = availableEntries;
            }

            for (std::uint32_t i = 0; i < count; ++i) {
                RE::FormID savedSource{};
                RE::FormID savedDestination{};
                if (a_skse->ReadRecordData(savedSource) != sizeof(savedSource) ||
                    a_skse->ReadRecordData(savedDestination) != sizeof(savedDestination)) {
                    logger::error("Redirect serialization entry {} is truncated", i);
                    break;
                }

                RE::FormID resolvedSource{};
                RE::FormID resolvedDestination{};
                if (!a_skse->ResolveFormID(savedSource, resolvedSource) ||
                    !a_skse->ResolveFormID(savedDestination, resolvedDestination)) {
                    logger::warn("Could not resolve redirect {:08X} -> {:08X}", savedSource, savedDestination);
                    continue;
                }

                auto* source = RE::TESForm::LookupByID<RE::TESObjectREFR>(resolvedSource);
                auto* destination = RE::TESForm::LookupByID<RE::TESObjectREFR>(resolvedDestination);
                if (!source || !destination) {
                    logger::warn("Could not find resolved redirect {:08X} -> {:08X}", resolvedSource, resolvedDestination);
                    continue;
                }

                if (manager->Add(source, destination)) ++restored;
            }
        }

        logger::info("Restored {} redirects", restored);
    }

    void RevertCallback([[maybe_unused]] SKSE::SerializationInterface* a_skse) {
        if (Settings::GetSingleton()->GSC_GlobalRedirects) {
            Settings::GetSingleton()->LoadRedirects();
        } else {
            RedirectManager::GetSingleton()->Clear();
            logger::info("Cleared redirects");
        }
    }
}
