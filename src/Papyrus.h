#pragma once

namespace Papyrus
{
	inline constexpr auto SCRIPT_NAME = "ArcaneVaults"sv;

	void ContainerRedirect(RE::StaticFunctionTag*);
	void OffloadItems(RE::StaticFunctionTag*);

	bool Register(RE::BSScript::IVirtualMachine* a_vm);
}
