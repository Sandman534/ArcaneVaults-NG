#include "Papyrus.h"

#include "Stores.h"
#include "Utility.h"

namespace Papyrus
{
	void ContainerRedirect(RE::StaticFunctionTag*) {
		Utility::GetSingleton()->ContainerRedirect();
	}

	void OffloadItems(RE::StaticFunctionTag*) {
		Stores::GetSingleton()->OffloadItems();
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm) {
		if (!a_vm) {
			logger::error("Failed to register Papyrus functions: virtual machine is null");
			return false;
		}

		a_vm->RegisterFunction("ContainerRedirect", SCRIPT_NAME, ContainerRedirect);
		a_vm->RegisterFunction("OffloadItems", SCRIPT_NAME, OffloadItems);

		logger::info("Registered Papyrus functions for {}", SCRIPT_NAME);
		return true;
	}
}
