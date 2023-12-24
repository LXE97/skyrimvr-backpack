#include "helper_game.h"

namespace helper
{
    using namespace RE;

    TESForm* LookupByName(FormType typeEnum, const char* name)
    {
        auto* data = TESDataHandler::GetSingleton();
        auto& forms = data->GetFormArray(typeEnum);
        for (auto*& form : forms)
        {
            if (!strcmp(form->GetName(), name))
            {
                return form;
            }
        }
        return nullptr;
    }

    float GetAVPercent(Actor* a, ActorValue v)
    {
        float current = a->AsActorValueOwner()->GetActorValue(v);
        float base = a->AsActorValueOwner()->GetBaseActorValue(v);
        float mod = a->GetActorValueModifier(ACTOR_VALUE_MODIFIER::kPermanent, v) +
            a->GetActorValueModifier(ACTOR_VALUE_MODIFIER::kTemporary, v);
        return current / (base + mod);
    }

    float GetChargePercent(Actor* a, bool isLeft)
    {
        if (auto equipped = a->GetEquippedObject(isLeft))
        {
            if (equipped->IsWeapon()) {
                float current = a->AsActorValueOwner()->GetActorValue(ActorValue::kRightItemCharge);

                // player made items
                if (auto entryData = a->GetEquippedEntryData(isLeft))
                {
                    for (auto& x : *(entryData->extraLists))
                    {
                        if (auto e = x->GetByType<ExtraEnchantment>())
                        {
                            return std::clamp(current / e->charge, 0.f, 100.f);
                        }
                    }
                }

                // prefab items
                if (auto ench = equipped->As<TESEnchantableForm>())
                {
                    return std::clamp(current / (float)(ench->amountofEnchantment), 0.f, 100.f);
                }
            }
        }
        return 0;
    }

    float GetGameHour()
    {
        if (auto c = Calendar::GetSingleton())
        {
            return c->GetHour();
        }
        return 0;
    }

    float GetAmmoPercent(Actor* a, float ammoCountMult)
    {
        if (auto ammo = a->GetCurrentAmmo())
        {
            auto countmap = a->GetInventoryCounts();
            if (countmap[ammo])
            {
                return std::clamp(countmap[ammo] * ammoCountMult, 0.f, 100.f);
            }
        }
        return 0;
    }

    float GetShoutCooldownPercent(Actor* a, float MaxCDTime)
    {
        return std::clamp(a->GetVoiceRecoveryTime() / MaxCDTime, 0.f, 100.f);
    }

    void SetGlowMult() {}

    void SetGlowColor(NiAVObject* target, NiColor* c)
    {
        auto geometry = target->GetFirstGeometryOfShaderType(RE::BSShaderMaterial::Feature::kGlowMap);
        if (geometry)
        {
            auto shaderProp = geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect].get();
            if (shaderProp)
            {
                auto shader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProp);
                if (shader)
                {
                    shader->emissiveColor = c;
                }
            }
        }
    }

    void SetSpecularMult() {}
    void SetSpecularColor() {}
    void SetTintColor() {}

    void CastSpellInstant(Actor* src, Actor* target, SpellItem* spell)
    {
        if (src && target && spell)
        {
            auto caster = src->GetMagicCaster(MagicSystem::CastingSource::kInstant);
            if (caster)
            {
                caster->CastSpellImmediate(spell, false, target, 1.0, false, 1.0, src);
            }
        }
    }

    void Dispel(Actor* src, Actor* target, SpellItem* spell)
    {
        if (src && target && spell) {
            auto handle = src->GetHandle();
            if (handle)
            {
                target->GetMagicTarget()->DispelEffect(spell, handle);
            }
        }
    }
}