#pragma once

namespace helper
{
    RE::TESForm* LookupByName(RE::FormType typeEnum, const char* name);

    void CastSpellInstant(RE::Actor* src, RE::Actor* target, RE::SpellItem* spell);
    void Dispel(RE::Actor* src, RE::Actor* target, RE::SpellItem* spell);

    float GetAVPercent(RE::Actor* a, RE::ActorValue v);
    float GetChargePercent(RE::Actor* a, bool isLeft);
    float GetGameHour();    //  24hr time
    float GetAmmoPercent(RE::Actor* a, float ammoCountMult);
    float GetShoutCooldownPercent(RE::Actor* a, float MaxCDTime);

    void SetGlowMult();
    void SetGlowColor();
    void SetSpecularMult();
    void SetSpecularColor();
    void SetTintColor();
}