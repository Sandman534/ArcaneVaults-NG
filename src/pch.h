#pragma once

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <fstream>

#ifdef NDEBUG
#include <spdlog/sinks/basic_file_sink.h>
#else
#include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)

// xbyak
#include <xbyak/xbyak.h>

using namespace std::literals;
using namespace REL::literals;

namespace logger = SKSE::log;

namespace stl 
{
	using namespace SKSE::stl;
}

namespace util 
{
	using SKSE::stl::report_and_fail;
}

#define DLLEXPORT __declspec(dllexport)

#define RELOCATION_OFFSET(SE, AE) REL::VariantOffset(SE, AE, 0).offset()

#include "plugin.h"
// #include "SKSEMenuFramework.h"
// #include "HelperFunctions.h"