#pragma once

namespace AVEvents {
	void Init(void);
	void RunAfterContainerMenuCloses(RE::TESObjectREFR* a_container, std::function<void()> a_callback);
	void CancelContainerMenuCloseCallback();
}
