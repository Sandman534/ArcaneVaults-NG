#include <stddef.h>
#include "Utility.h"
#include "Events.h"
#include "ContainerMenu.h"
#include "Compatibility.h"
#include "Serialization.h"
#include "Settings.h"
#include "AVUI.h"
#include "Translation.h"

using namespace RE::BSScript;
using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message) {
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		AVTranslation::Install();
		Utility::GetSingleton()->LoadAllForms();
		Settings::GetSingleton()->Init();
		Events::Init();
		Menus::Init();
		AVUI::Register();		
		break;
	case SKSE::MessagingInterface::kPostPostLoad:
		QuickLoot::QuickLootAPI::Init();
		if (QuickLoot::QuickLootAPI::IsReady() && QuickLoot::QuickLootAPI::RegisterOpeningLootMenuHandler(OnOpeningQuickLootMenu))
			logger::info("QuickLoot IE integration enabled");
		else
			logger::warn("QuickLoot IE API unavailable");
		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	REL::Module::reset();
	 
	// Message Listener
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));

	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}
	 
	logger::info("{} v{} is loading..."sv, Plugin::NAME, Plugin::VERSION.string());
	 
	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(128);
	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	// Serialization
	if (auto* serialization = SKSE::GetSerializationInterface()) {
		serialization->SetUniqueID(Serialization::ID);
		serialization->SetSaveCallback(&Serialization::SaveCallback);
		serialization->SetLoadCallback(&Serialization::LoadCallback);
		serialization->SetRevertCallback(&Serialization::RevertCallback);
	} else {
		logger::critical("Failed to get serialization interface");
		return false;
	}

	logger::info("{} has finished loading.", Plugin::NAME);
	return true;
}
