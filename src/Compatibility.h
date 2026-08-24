#pragma once

#include "QuickLootAPI.h"
#include "RedirectManager.h"

using namespace QuickLoot;

namespace
{
    void OnOpeningQuickLootMenu(OpeningLootMenuEvent* event) {
        if (!event) return;

        auto container = event->container;
        if (!container) return;

        // Block QuickLoot for redirected containers.
        if (RedirectManager::GetSingleton()->FindDestination(container))
            event->result = HandleResult::kStop;
    }
}
