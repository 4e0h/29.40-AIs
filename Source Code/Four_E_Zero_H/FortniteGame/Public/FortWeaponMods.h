#pragma once
#include "../../pch.h"
#include "FortInventory.h"
#include "FortWeapon.h"
#include "GameplayTagContainer.h"

class AFortPlayerControllerAthena;

class UFortWeaponModItemDefinition : public UFortItemDefinition
{
public:
    UCLASS_COMMON_MEMBERS(UFortWeaponModItemDefinition);

    DEFINE_PROP(ModSlot, FGameplayTag);
};

class UFortWeaponModFunctionLibrary : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UFortWeaponModFunctionLibrary);

    DEFINE_STATIC_FUNC(TryAddWeaponMod, bool);
    DEFINE_STATIC_FUNC(ModAllowedOnWeaponDefinition, bool);
    DEFINE_STATIC_FUNC(ApplyWeaponModToPickup, bool);
};

// chapter 5 weapon mods (optics, barrels, underbarrels, magazines), ported from Boron (github.com/Ducki67/Boron).
namespace WeaponMods
{
    void InitEntrySlots(const UFortItemDefinition* Definition, FFortItemEntry& Entry);

    void OnItemGiven(AFortInventory* Inventory, FFortItemEntry& Source, UFortWorldItem* Item);

    void OnPickupSpawned(AFortPickupAthena* Pickup, FFortItemEntry& Source);

    void OnWeaponEquipped(AFortPlayerControllerAthena* PlayerController, AActor* Weapon);

    bool HandleCheat(AFortPlayerControllerAthena* PlayerController, const std::string& Command, const std::string& Argument);

    void Hook();
}
