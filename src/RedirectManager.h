#pragma once

class RedirectManager {
public:
    struct ContainerRedirect {
        RE::FormID destinationID{ 0 };
    };
    
    static RedirectManager* GetSingleton() {
        static RedirectManager singleton;
        return &singleton;
    }

    bool Add(RE::TESObjectREFR* source, RE::TESObjectREFR* destination);
    bool Remove(RE::TESObjectREFR* source);
    void Clear();

    [[nodiscard]] RE::TESObjectREFR* FindDestination(const RE::TESObjectREFR* source) const;
    [[nodiscard]] const std::unordered_map<RE::FormID, ContainerRedirect>& GetRedirects() const noexcept {
        return redirects;
    }

private:
    std::unordered_map<RE::FormID, ContainerRedirect> redirects;
};
