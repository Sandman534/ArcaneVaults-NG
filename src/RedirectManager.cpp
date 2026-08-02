#include "RedirectManager.h"

namespace
{
    void RefreshCrosshairText()
    {
        SKSE::GetTaskInterface()->AddTask([] {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) return;

            player->GetPlayerRuntimeData().playerFlags.shouldUpdateCrosshair = true;
            player->UpdateCrosshairs();
        });
    }

    void SetLinkedContainerBlocked(RE::TESObjectREFR* a_source, bool a_blocked)
    {
        if (!a_source || !a_source->GetBaseObject() ||
            !a_source->GetBaseObject()->As<RE::TESObjectACTI>()) {
            return;
        }

        auto* linked = a_source->GetLinkedRef(nullptr);
        if (linked && linked->GetBaseObject() &&
            linked->GetBaseObject()->As<RE::TESObjectCONT>()) {
            linked->SetActivationBlocked(a_blocked);
        }
    }
}

bool RedirectManager::Add(RE::TESObjectREFR* source, RE::TESObjectREFR* destination) {
    if (!source || !destination) return false;
    const auto sourceID = source->GetFormID();

    // Preserve the name only when initially creating the redirect.
    auto [entry, inserted] = redirects.try_emplace(sourceID, RedirectManager::ContainerRedirect{ destination->GetFormID() });

    // Update an existing redirect without overwriting its original name.
    if (!inserted)
        entry->second.destinationID = destination->GetFormID();

    // A script on an ACTI may activate its linked container later.
    SetLinkedContainerBlocked(source, true);

    // Set the display name of the source to show to our redirect
    RefreshCrosshairText();

    return true;
}

bool RedirectManager::Remove(RE::TESObjectREFR* source) {
    if (!source) return false;

    // Get the source redirect
    const auto entry = redirects.find(source->GetFormID());
    if (entry == redirects.end()) return false;

    // Return the activator back to what it was
    SetLinkedContainerBlocked(source, false);
    // Remove the entry
    redirects.erase(entry);

    // Set the display name back to the original
    RefreshCrosshairText();

    return true;
}

void RedirectManager::Clear() {
    for (const auto& [sourceID, redirect] : redirects) {
        if (auto* source = RE::TESForm::LookupByID<RE::TESObjectREFR>(sourceID)) {
            SetLinkedContainerBlocked(source, false);
        }
    }

    redirects.clear();
    RefreshCrosshairText();
}

RE::TESObjectREFR* RedirectManager::FindDestination(const RE::TESObjectREFR* source) const {
    if (!source) return nullptr;

    // Find the source object in our map
    const auto entry = redirects.find(source->GetFormID());
    if (entry == redirects.end()) return nullptr;

    // Return the found redirected form
    return RE::TESForm::LookupByID<RE::TESObjectREFR>(entry->second.destinationID);
}
