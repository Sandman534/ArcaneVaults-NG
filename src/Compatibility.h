#pragma once

#include "QuickLootAPI.h"
#include "RedirectManager.h"

using namespace QuickLoot::API;

namespace
{
    void OnOpeningQuickLootMenu(OpeningLootMenuEvent* event) {
        if (!event) return;

        auto container = event->container.get();
        if (!container) return;

        // Block QuickLoot for redirected containers.
        if (RedirectManager::GetSingleton()->FindDestination(container.get()))
            event->result = HandleResult::kStop;
    }
}
