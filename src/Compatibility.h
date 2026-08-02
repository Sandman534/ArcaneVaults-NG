#pragma once

#include "QuickLootAPI.h"
#include "RedirectManager.h"

namespace
{
    void OnOpeningQuickLootMenu(QuickLoot::OpeningLootMenuEvent* event) {
        if (!event) return;

        auto* container = event->container;
        if (!container) return;

        // Block QuickLoot for redirected containers.
        if (RedirectManager::GetSingleton()->FindDestination(container))
            event->result = QuickLoot::HandleResult::kStop;
    }
}
