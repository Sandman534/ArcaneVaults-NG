#pragma once

namespace Serialization
{
	static constexpr std::uint32_t SerializationVersion = 1;
	static constexpr std::uint32_t ID = 'AVLS';
	static constexpr std::uint32_t SerializationType = 'RDIR';

	void SaveCallback(SKSE::SerializationInterface* a_skse);
	void LoadCallback(SKSE::SerializationInterface* a_skse);
	void RevertCallback([[maybe_unused]] SKSE::SerializationInterface* a_skse);
}
