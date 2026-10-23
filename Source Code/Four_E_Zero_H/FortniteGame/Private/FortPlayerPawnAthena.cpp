#include "pch.h"
#include "../Public/FortPlayerPawnAthena.h"
#include "../Public/FortInventory.h"
#include "../Public/FortPhysicsPawn.h"
#include "../Public/FortPlayerControllerAthena.h"
#include "../Public/FortWeapon.h"
#include "../../Four_E_Zero_H/Public/Bots.h"

struct _Pad_0xC
{
    uint8_t Padding[0xC];
};

struct _Pad_0x18
{
    uint8_t Padding[0x18];
};

struct FFortPickupRequestInfo final
{
public:
    struct FGuid SwapWithItem;
    float FlyTime;
    struct _Pad_0xC Direction;
    uint8 bPlayPickupSound : 1;
    uint8 bIsAutoPickup : 1;
    uint8 bUseRequestedSwap : 1;
    uint8 bTrySwapWithWeapon : 1;
    uint8 Pad_21[0x3];
};

struct alignas(0x8) FFortPickupRequestInfoNew final
{
public:
    struct FGuid SwapWithItem;
    float FlyTime;
    uint8 Pad_1[0x4];
    struct _Pad_0x18 Direction;
    uint8 bPlayPickupSound : 1;
    uint8 bIsAutoPickup : 1;
    uint8 bUseRequestedSwap : 1;
    uint8 bTrySwapWithWeapon : 1;
    uint8 Pad_2[0x7];
};

uint64_t SetPickupTarget_ = 0;
void AFortPlayerPawnAthena::ServerHandlePickup_(UObject* Context, FFrame& Stack)
{
    AFortPickupAthena* Pickup;
    float InFlyTime;
    FVector InStartDirection;
    bool bPlayPickupSound;
    Stack.StepCompiledIn(&Pickup);
    Stack.StepCompiledIn(&InFlyTime);
    Stack.StepCompiledIn(&InStartDirection);
    Stack.StepCompiledIn(&bPlayPickupSound);
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;
    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;

    auto SetPickupTarget = (void (*&)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector, bool))SetPickupTarget_;

    SetPickupTarget(Pickup, Pawn, InFlyTime / (Pawn->HasPickupSpeedMultiplier() ? Pawn->PickupSpeedMultiplier : 1), InStartDirection, bPlayPickupSound);
}

void AFortPlayerPawnAthena::ServerHandlePickupInfo(UObject* Context, FFrame& Stack)
{
    bool bTrySwapWithWeapon;
    bool bUseRequestedSwap;
    bool bPlayPickupSound;
    bool bIsAutoPickup;
    FGuid SwapWithItem;
    float FlyTime;
    FVector Direction;

    AFortPickupAthena* Pickup;
    Stack.StepCompiledIn(&Pickup);
    if (VersionInfo.FortniteVersion >= 20.00)
    {
        FFortPickupRequestInfoNew Params;
        Stack.StepCompiledIn(&Params);
        bTrySwapWithWeapon = Params.bTrySwapWithWeapon;
        bUseRequestedSwap = Params.bUseRequestedSwap;
        bPlayPickupSound = Params.bPlayPickupSound;
        bIsAutoPickup = Params.bIsAutoPickup;
        SwapWithItem = Params.SwapWithItem;
        FlyTime = Params.FlyTime;
        Direction = *(FVector*)&Params.Direction;
    }
    else
    {
        FFortPickupRequestInfo Params;
        Stack.StepCompiledIn(&Params);
        bTrySwapWithWeapon = Params.bTrySwapWithWeapon;
        bUseRequestedSwap = Params.bUseRequestedSwap;
        bPlayPickupSound = Params.bPlayPickupSound;
        bIsAutoPickup = Params.bIsAutoPickup;
        SwapWithItem = Params.SwapWithItem;
        FlyTime = Params.FlyTime;
        Direction = *(FVector*)&Params.Direction;
    }
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;

    if (bIsAutoPickup && Pawn->Controller && Pawn->Controller->IsA<AFortPlayerControllerAthena>())
    {
        static int Ignored = 0;
        if (++Ignored <= 5 || (Ignored % 100) == 0)
        {
            printf("[PICKUP] Ignored a walk-over auto pickup from a player (%d so far), they pick up with E.\n", Ignored);
            fflush(stdout);
        }
        return;
    }

    if (bUseRequestedSwap && Pawn->CurrentWeapon && AFortInventory::IsPrimaryQuickbar(((AFortWeapon*)Pawn->CurrentWeapon)->WeaponData) &&
        AFortInventory::IsPrimaryQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition))
    {
        auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

        PlayerController->bTryPickupSwap = true;
    }

    auto SetPickupTarget = (void (*&)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector&, bool))SetPickupTarget_;

    SetPickupTarget(Pickup, Pawn, FlyTime / (Pawn->HasPickupSpeedMultiplier() ? Pawn->PickupSpeedMultiplier : 1), Direction, bPlayPickupSound);
}

void AFortPlayerPawnAthena::ServerHandlePickupWithRequestedSwap(UObject* Context, FFrame& Stack)
{
    AFortPickupAthena* Pickup;
    FGuid Swap;
    float InFlyTime;
    FVector InStartDirection;
    bool bPlayPickupSound;
    Stack.StepCompiledIn(&Pickup);
    Stack.StepCompiledIn(&Swap);
    Stack.StepCompiledIn(&InFlyTime);
    Stack.StepCompiledIn(&InStartDirection);
    Stack.StepCompiledIn(&bPlayPickupSound);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;
    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    PlayerController->bTryPickupSwap = true;

    auto SetPickupTarget = (void (*&)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector&, bool))SetPickupTarget_;

    SetPickupTarget(Pickup, Pawn, InFlyTime / (Pawn->HasPickupSpeedMultiplier() ? Pawn->PickupSpeedMultiplier : 1), InStartDirection, bPlayPickupSound);
}

bool AFortPlayerPawnAthena::FinishedTargetSpline(void* _Pickup)
{
    auto Pickup = (AFortPickupAthena*)_Pickup;

    auto Pawn = (AFortPlayerPawnAthena*)Pickup->PickupLocationData.PickupTarget;
    if (!Pawn)
        return FinishedTargetSplineOG(Pickup);

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
    if (!PlayerController)
        return FinishedTargetSplineOG(Pickup);

    if (!PlayerController->IsA<AFortPlayerControllerAthena>())
    {
        Bots::OnPickup(Pawn->Controller, Pawn, &Pickup->PrimaryPickupItemEntry);
        return FinishedTargetSplineOG(Pickup);
    }

    if (PlayerController->HasbTryPickupSwap() ? PlayerController->bTryPickupSwap : false)
    {
        FVector FinalLoc = Pawn->K2_GetActorLocation();

        FVector ForwardVector = Pawn->GetActorForwardVector();
        ForwardVector.Z = 0.0f;
        ForwardVector.Normalize();

        FinalLoc = FinalLoc + ForwardVector * 450.f;
        FinalLoc.Z += 50.f;

        const float RandomAngleVariation = ((float)rand() * 0.00109866634f) - 18.f;
        const float FinalAngle = RandomAngleVariation * 0.017453292519943295f;

        FinalLoc.X += cos(FinalAngle) * 100.f;
        FinalLoc.Y += sin(FinalAngle) * 100.f;

        if (AFortInventory::IsPrimaryQuickbar(((AFortWeapon*)Pawn->CurrentWeapon)->WeaponData) && AFortInventory::IsPrimaryQuickbar(Pickup->PrimaryPickupItemEntry.ItemDefinition))
        {
            PlayerController->bTryPickupSwap = false;

            auto entry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
            { return entry.ItemGuid == ((AFortWeapon*)PlayerController->Pawn->CurrentWeapon)->ItemEntryGuid; }, FFortItemEntry::Size());

            AFortInventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), *entry, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(), PlayerController->MyFortPawn, -1,
                                        true, true, true, nullptr, FinalLoc);

            PlayerController->WorldInventory->Remove(entry->ItemGuid);
            auto Item = PlayerController->WorldInventory->GiveItem(Pickup->PrimaryPickupItemEntry);
            PlayerController->ServerExecuteInventoryItem(Item->ItemEntry.ItemGuid);
        }
        else
            AFortInventory::SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), Pickup->PrimaryPickupItemEntry, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(),
                                        PlayerController->MyFortPawn, -1, true, true, true, nullptr, FinalLoc);
    }
    else
        PlayerController->InternalPickup(&Pickup->PrimaryPickupItemEntry);

    return FinishedTargetSplineOG(Pickup);
}

uint64_t OnRep_ZiplineState = 0;
void AFortPlayerPawnAthena::ServerSendZiplineState(UObject* Context, FFrame& Stack)
{
    FZiplinePawnState State;

    Stack.StepCompiledIn(&State);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn)
        return;

    auto Zipline = Pawn->GetActiveZipline();

    auto PreviousState = Pawn->ZiplineState;

    memcpy((PBYTE)&Pawn->ZiplineState, (const PBYTE)&State, FZiplinePawnState::Size());

    if (OnRep_ZiplineState)
        ((void (*)(AFortPlayerPawnAthena*))OnRep_ZiplineState)(Pawn);

    if (State.bJumped)
    {
        auto Velocity = Pawn->CharacterMovement->Velocity;
        auto VelocityX = Velocity.X * -0.5f;
        auto VelocityY = Velocity.Y * -0.5f;
        Pawn->LaunchCharacterJump(FVector{ VelocityX >= -750 ? min(VelocityX, 750) : -750, VelocityY >= -750 ? min(VelocityY, 750) : -750, 1200 }, false, false, true, true);
    }

    auto NewZipline = Pawn->GetActiveZipline();

    static auto ZipLineClass = FindObject<UClass>(L"/Ascender/Gameplay/Ascender/B_Athena_Zipline_Ascender.B_Athena_Zipline_Ascender_C");
    if (auto Ascender = Zipline->Cast<AFortAscenderZipline>(ZipLineClass))
    {
        Ascender->PawnUsingHandle = nullptr;
        Ascender->PreviousPawnUsingHandle = Pawn;
        Ascender->OnRep_PawnUsingHandle();
    }
    else if (auto Ascender = NewZipline->Cast<AFortAscenderZipline>(ZipLineClass))
    {
        Ascender->PawnUsingHandle = Pawn;
        Ascender->PreviousPawnUsingHandle = nullptr;
        Ascender->OnRep_PawnUsingHandle();
    }
}

void AFortPlayerPawnAthena::OnCapsuleBeginOverlap_(UObject* Context, FFrame& Stack)
{
    UObject* OverlappedComp;
    AActor* OtherActor;
    UObject* OtherComp;
    int32 OtherBodyIndex;
    bool bFromSweep;
    struct
    {
        uint8_t Padding[0x100];
    } SweepResult;
    Stack.StepCompiledIn(&OverlappedComp);
    Stack.StepCompiledIn(&OtherActor);
    Stack.StepCompiledIn(&OtherComp);
    Stack.StepCompiledIn(&OtherBodyIndex);
    Stack.StepCompiledIn(&bFromSweep);
    Stack.StepCompiledIn(&SweepResult);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    static auto FortPCClass = FindClass("FortPlayerController");

    if (!Pawn || !Pawn->Controller || !Pawn->Controller->IsA(FortPCClass))
    {
        if (Pawn && Pawn->Controller && OtherActor && OtherActor->Cast<AFortPickupAthena>() && !Bots::WantsPickup(Pawn->Controller, OtherActor))
            return;
        return callOG(Pawn, Stack.GetCurrentNativeFunction(), OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
    }

    if (OtherActor && OtherActor->Cast<AFortPickupAthena>())
        return;

    return callOG(Pawn, Stack.GetCurrentNativeFunction(), OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void AFortPlayerPawnAthena::MovingEmoteStopped(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (Pawn->HasbIsPlayingEmote() && Pawn->bIsPlayingEmote)
        return;

    static auto HasbMovingEmote = Pawn->HasbMovingEmote();
    if (HasbMovingEmote)
        Pawn->bMovingEmote = false;

    static auto HasbMovingEmoteForwardOnly = Pawn->HasbMovingEmoteForwardOnly();
    if (HasbMovingEmoteForwardOnly)
        Pawn->bMovingEmoteForwardOnly = false;

    static auto HasbMovingEmoteFollowingOnly = Pawn->HasbMovingEmoteFollowingOnly();
    if (HasbMovingEmoteFollowingOnly)
        Pawn->bMovingEmoteFollowingOnly = false;

    if (Pawn->HasLastReplicatedEmoteExecuted())
    {
        auto OldEmote = Pawn->LastReplicatedEmoteExecuted;
        Pawn->LastReplicatedEmoteExecuted = nullptr;
        Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
    }
}

class UGA_Athena_MedConsumable_Parent_C : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UGA_Athena_MedConsumable_Parent_C);

    DEFINE_PROP(PlayerPawn, AFortPlayerPawnAthena*);
    DEFINE_PROP(HealsShields, bool);
    DEFINE_PROP(HealsHealth, bool);
    DEFINE_PROP(HealthHealAmount, float);
};

void AFortPlayerPawnAthena::Athena_MedConsumable_Triggered(UObject* Context, FFrame& Stack)
{
    UGA_Athena_MedConsumable_Parent_C* Consumable = (UGA_Athena_MedConsumable_Parent_C*)Context;

    printf("Called yo\n");
    if (!Consumable || (!Consumable->HealsShields && !Consumable->HealsHealth) || !Consumable->PlayerPawn)
        return Athena_MedConsumable_TriggeredOG(Context, Stack);

    auto PlayerState = (AFortPlayerStateAthena*)Consumable->PlayerPawn->PlayerState;
    static auto ShieldCue = FName(L"GameplayCue.Shield.PotionConsumed");
    static auto HealthCue = FName(L"GameplayCue.Athena.Health.HealUsed");

    auto Handle = PlayerState->AbilitySystemComponent->MakeEffectContext();
    FGameplayTag Tag{};
    FName CueName = Consumable->HealsShields ? ShieldCue : HealthCue;

    if (Consumable->HealsHealth && Consumable->HealsShields)
    {
        static auto HealthHealAmountOffset = Consumable->GetOffset("HealthHealAmount");
        auto HealthHealAmount = Consumable->HasHealthHealAmount() ? *(float*)(__int64(Consumable) + HealthHealAmountOffset) : *(double*)(__int64(Consumable) + HealthHealAmountOffset);
        if (Consumable->PlayerPawn->GetHealth() + HealthHealAmount <= 100)
            CueName = HealthCue;
    }
    Tag.TagName = CueName;

    auto PredictionKey = (FPredictionKey*)malloc(FPredictionKey::Size());
    memset((PBYTE)PredictionKey, 0, FPredictionKey::Size());

    PlayerState->AbilitySystemComponent->NetMulticast_InvokeGameplayCueAdded(Tag, *PredictionKey, Handle);
    PlayerState->AbilitySystemComponent->NetMulticast_InvokeGameplayCueExecuted(Tag, *PredictionKey, Handle);

    free(PredictionKey);

    return Athena_MedConsumable_TriggeredOG(Context, Stack);
}

void AFortPlayerPawnAthena::ServerOnExitVehicle_(UObject* Context, FFrame& Stack, AActor** Ret)
{
    struct FVehicleExitData
    {
        uint8_t Pad[0x30];
    };

    FVehicleExitData VehicleExitData;
    uint8_t ExitForceBehavior;
    bool bDestroyVehicleWhenForced;
    if (VersionInfo.FortniteVersion >= 29)
        Stack.StepCompiledIn(&VehicleExitData);
    else
    {
        Stack.StepCompiledIn(&ExitForceBehavior);
        Stack.StepCompiledIn(&bDestroyVehicleWhenForced);
    }

    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    static auto GetVehicleFunc = Pawn->GetFunction("GetVehicleActor");
    if (!GetVehicleFunc)
        GetVehicleFunc = Pawn->GetFunction("GetVehicle");
    auto Vehicle = Pawn->Call<AActor*>(GetVehicleFunc);

    if (!Vehicle && Pawn->IsA<AFortCharacterVehicle>())
        Vehicle = Pawn;

    if (!Vehicle)
    {
        if (VersionInfo.FortniteVersion >= 29)
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, VehicleExitData);
        else
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);
    }

    UFortVehicleSeatWeaponComponent* SeatWeaponComponent = (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass());

    if (!SeatWeaponComponent)
    {
        printf("nop %s\n", Pawn ? Pawn->Class->Name.ToString().c_str() : nullptr);
        if (VersionInfo.FortniteVersion >= 29)
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, VehicleExitData);
        else
            return callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);
    }

    UFortVehicleSeatComponent* SeatComponent = (UFortVehicleSeatComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatComponent::StaticClass());

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    auto SeatIdx = SeatComponent->FindSeatIndex(Pawn);

    UFortWeaponItemDefinition* Weapon = nullptr;
    if (SeatWeaponComponent)
    {
        for (int i = 0; i < SeatWeaponComponent->WeaponSeatDefinitions.Num(); i++)
        {
            auto& WeaponDefinition = SeatWeaponComponent->WeaponSeatDefinitions.Get(i, FWeaponSeatDefinition::Size());

            if (WeaponDefinition.SeatIndex != SeatIdx)
                continue;

            Weapon = WeaponDefinition.VehicleWeapon;
            break;
        }

        if (Weapon)
        {
            auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([Weapon](FFortItemEntry& entry) { return entry.ItemDefinition == Weapon; }, FFortItemEntry::Size());

            if (ItemEntry)
                PlayerController->WorldInventory->Remove(ItemEntry->ItemGuid);
        }
    }
    if (VersionInfo.FortniteVersion >= 29)
        *Ret = callOGWithRet(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, VehicleExitData);
    else
        *Ret = callOGWithRet(Pawn, Stack.GetCurrentNativeFunction(), ServerOnExitVehicle, ExitForceBehavior, bDestroyVehicleWhenForced);

    if (Weapon)
    {
        auto LastItem = Pawn->HasPreviousWeapon() ? (AFortWeapon*)Pawn->PreviousWeapon : nullptr;

        if (LastItem)
        {
            PlayerController->ServerExecuteInventoryItem(LastItem->ItemEntryGuid);
            PlayerController->ClientEquipItem(LastItem->ItemEntryGuid, true);
        }
        else
        {
            printf("yo\n");
            auto pickaxeEntry =
                PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([](FFortItemEntry& entry) { return entry.ItemDefinition->IsA<UFortWeaponMeleeItemDefinition>(); }, FFortItemEntry::Size());

            if (pickaxeEntry)
            {
                printf("yo2\n");
                PlayerController->ServerExecuteInventoryItem(pickaxeEntry->ItemGuid);
                PlayerController->ClientEquipItem(pickaxeEntry->ItemGuid, true);
            }
        }
    }
}

void AFortPlayerPawnAthena::EmoteStopped_(UObject* Context, FFrame& Stack)
{
    UObject* MontageItemDef;

    Stack.StepCompiledIn(&MontageItemDef);
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (Pawn->HasLastReplicatedEmoteExecuted() && Pawn->LastReplicatedEmoteExecuted == MontageItemDef)
    {
        auto OldEmote = Pawn->LastReplicatedEmoteExecuted;
        Pawn->LastReplicatedEmoteExecuted = nullptr;
        Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
    }

    return callOG(Pawn, Stack.GetCurrentNativeFunction(), EmoteStopped, MontageItemDef);
}

void AFortPlayerPawnAthena::EndSkydiving(AFortPlayerPawnAthena* Pawn)
{
    EndSkydivingOG(Pawn);

    if (!Pawn || !Pawn->Controller || !Pawn->Controller->IsA<AFortPlayerControllerAthena>())
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
    auto QuestManager = PlayerController->GetQuestManager(1);
    if (!QuestManager)
        return;

    if (Pawn->bIsSkydiving)
        QuestManager->SendStatEvent(PlayerController, EFortQuestObjectiveStatEvent::GetLand(), 1, Pawn);

    if (Pawn->bIsSkydivingFromBus)
        QuestManager->SendStatEvent(PlayerController, EFortQuestObjectiveStatEvent::GetVisit(), 1, Pawn);
}

void AFortPlayerPawnAthena::ServerReviveFromDBNO(UObject* Context, FFrame& Stack)
{
    AFortPlayerControllerAthena* EventInstigator;

    Stack.StepCompiledIn(&EventInstigator);
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;
}

void AFortPlayerPawnAthena::ServerThrowCarriedPlayer_(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();
    auto Pawn = (AFortPlayerPawnAthena*)Context;

    callOG(Pawn, Stack.GetCurrentNativeFunction(), ServerThrowCarriedPlayer);
    Pawn->LocalThrowCarriedPlayer();
}

void AFortPlayerPawnAthena::SetIsInsideSafeZone(AFortPlayerPawnAthena* _this, bool bNewValue)
{
    if (_this->bIsInsideSafeZone == bNewValue)
        return;

    _this->bIsInsideSafeZone = bNewValue;
    _this->OnRep_IsInsideSafeZone();
}

void AFortPlayerPawnAthena::UpdateOutsideSafeZone(AFortPlayerPawnAthena* _this)
{
    if (_this->bIsInsideSafeZone == !_this->bIsInAnyStorm)
        return;

    _this->bIsInsideSafeZone = !_this->bIsInAnyStorm;
    _this->OnRep_IsInsideSafeZone();
}

namespace PlayerHits
{
    struct FField
    {
        int32 Offset = -1;
        uint32 Size = 0;

        bool Fits(uint32 ContainerSize) const
        {
            return Offset >= 0 && Size > 0 && (uint32)Offset + Size <= ContainerSize;
        }
    };

    static FField Param(const UFunction* Function, const char* Name)
    {
        if (!Function)
            return {};
        auto Params = Function->GetParamsNamed();
        for (auto& Entry : Params.NameOffsetMap)
            if (Entry.Name == Name)
                return { (int32)Entry.Offset, Entry.ElementSize };
        return {};
    }

    static uint32 ParamsSize(const UFunction* Function)
    {
        return Function ? (uint32)Function->GetPropertiesSize() : 0;
    }

    static FField Field(const UStruct* Struct, const char* Name)
    {
        auto Prop = Struct ? Struct->GetProperty(Name) : nullptr;
        if (!Prop)
            return {};
        return { (int32)GetFromOffset<uint32>(Prop, Offsets::Offset_Internal), GetFromOffset<uint32>(Prop, Offsets::ElementSize) };
    }

    static bool Write(uint8* Params, uint32 Size, const FField& Where, const void* Value, uint32 ValueSize)
    {
        if (!Where.Fits(Size) || Where.Size != ValueSize)
            return false;
        memcpy(Params + Where.Offset, Value, ValueSize);
        return true;
    }

    template <typename T>
    static bool WriteValue(uint8* Params, uint32 Size, const FField& Where, const T& Value)
    {
        return Write(Params, Size, Where, &Value, (uint32)sizeof(T));
    }

    template <typename T>
    static bool ReadValue(const uint8* Params, uint32 Size, const FField& Where, T& Out)
    {
        if (!Where.Fits(Size) || Where.Size != sizeof(T))
            return false;
        memcpy(&Out, Params + Where.Offset, sizeof(T));
        return true;
    }

    struct FApi
    {
        bool bTried = false;
        bool bReady = false;

        FField NotifyHit, NotifyOrigin;

        uint32 HitSize = 0;
        FField HitComponent, HitActor, HitHandle, HitImpactPoint, HitLocation;
        FField HandleObject;

        UFunction* MakeContext = nullptr;
        FField MakeSource, MakeCauser, MakeDamageSource, MakeLevel, MakeReturn;

        UObject* Library = nullptr;
        UFunction* AddHit = nullptr;
        FField AddHitContext, AddHitResult, AddHitReset;
        UFunction* SetOrigin = nullptr;
        FField SetOriginContext, SetOriginValue;

        UObject* FortLibrary = nullptr;
        UFunction* SetCritical = nullptr;
        FField SetCriticalContext, SetCriticalValue;

        UFunction* Apply = nullptr;
        FField ApplyClass, ApplyTarget, ApplySource, ApplyCauser, ApplyDamageSource, ApplyLevel, ApplyContext, ApplyReturn;

        UFunction* DamageZone = nullptr;
        FField DamageZoneHit, DamageZoneReturn;

        uint32 ContainerSize = 0;
        FField ContainerEffects;
    };

    static FApi Api;

    static bool Resolve(const UFunction* NotifyFunction)
    {
        if (Api.bTried)
            return Api.bReady;
        Api.bTried = true;

        Api.NotifyHit = Param(NotifyFunction, "Hit");
        if (!Api.NotifyHit.Fits(ParamsSize(NotifyFunction)))
            Api.NotifyHit = Param(NotifyFunction, "HitResult");
        Api.NotifyOrigin = Param(NotifyFunction, "ProjectileOriginPosition");

        if (auto HitStruct = FindStruct("HitResult"))
        {
            Api.HitSize = (uint32)HitStruct->GetPropertiesSize();
            Api.HitComponent = Field(HitStruct, "Component");
            Api.HitActor = Field(HitStruct, "Actor");
            Api.HitHandle = Field(HitStruct, "HitObjectHandle");
            Api.HitImpactPoint = Field(HitStruct, "ImpactPoint");
            Api.HitLocation = Field(HitStruct, "Location");
        }
        if (auto HandleStruct = FindStruct("ActorInstanceHandle"))
        {
            Api.HandleObject = Field(HandleStruct, "ReferenceObject");
            if (Api.HandleObject.Size != sizeof(FWeakObjectPtr))
                Api.HandleObject = Field(HandleStruct, "Actor");
        }

        auto FortAbilitySystemClass = FindClass("FortAbilitySystemComponent");
        auto FortAbilitySystem = FortAbilitySystemClass ? FortAbilitySystemClass->GetDefaultObj() : nullptr;
        if (FortAbilitySystem)
        {
            Api.MakeContext = FortAbilitySystem->GetFunction("MakeFortEffectContext");
            Api.Apply = FortAbilitySystem->GetFunction("BP_FortApplyGameplayEffectToTarget");
        }

        auto LibraryClass = FindClass("AbilitySystemBlueprintLibrary");
        Api.Library = LibraryClass ? LibraryClass->GetDefaultObj() : nullptr;
        if (Api.Library)
        {
            Api.AddHit = Api.Library->GetFunction("EffectContextAddHitResult");
            Api.SetOrigin = Api.Library->GetFunction("EffectContextSetOrigin");
        }

        auto FortLibraryClass = FindClass("FortAbilitySystemBlueprintLibrary");
        Api.FortLibrary = FortLibraryClass ? FortLibraryClass->GetDefaultObj() : nullptr;
        if (Api.FortLibrary)
            Api.SetCritical = Api.FortLibrary->GetFunction("EffectContextSetCritical");

        Api.MakeSource = Param(Api.MakeContext, "SourceObject");
        Api.MakeCauser = Param(Api.MakeContext, "EffectCauser");
        Api.MakeDamageSource = Param(Api.MakeContext, "DamageSource");
        Api.MakeLevel = Param(Api.MakeContext, "GameplayEffectLevel");
        Api.MakeReturn = Param(Api.MakeContext, "ReturnValue");

        Api.AddHitContext = Param(Api.AddHit, "EffectContext");
        Api.AddHitResult = Param(Api.AddHit, "HitResult");
        Api.AddHitReset = Param(Api.AddHit, "bReset");
        Api.SetOriginContext = Param(Api.SetOrigin, "EffectContext");
        Api.SetOriginValue = Param(Api.SetOrigin, "Origin");
        Api.SetCriticalContext = Param(Api.SetCritical, "EffectContext");
        Api.SetCriticalValue = Param(Api.SetCritical, "bInCritical");

        Api.ApplyClass = Param(Api.Apply, "GameplayEffectClass");
        Api.ApplyTarget = Param(Api.Apply, "TargetOfEffect");
        Api.ApplySource = Param(Api.Apply, "OptionalSourceObject");
        Api.ApplyCauser = Param(Api.Apply, "OptionalEffectCauser");
        Api.ApplyDamageSource = Param(Api.Apply, "OptionalDamageSource");
        Api.ApplyLevel = Param(Api.Apply, "Level");
        Api.ApplyContext = Param(Api.Apply, "EffectContext");
        Api.ApplyReturn = Param(Api.Apply, "ReturnValue");

        if (auto PawnDefault = AFortPlayerPawnAthena::GetDefaultObj())
            Api.DamageZone = PawnDefault->GetFunction("GetDamageZone");
        Api.DamageZoneHit = Param(Api.DamageZone, "InHitResult");
        Api.DamageZoneReturn = Param(Api.DamageZone, "ReturnValue");

        if (auto ContainerStruct = FindStruct("FortGameplayEffectContainer"))
        {
            Api.ContainerSize = (uint32)ContainerStruct->GetPropertiesSize();
            Api.ContainerEffects = Field(ContainerStruct, "TargetGameplayEffectClasses");
        }

        const uint32 ContextSize = Api.MakeReturn.Size;
        Api.bReady = Api.NotifyHit.Size == Api.HitSize && Api.HitSize > 0 && Api.HitSize <= 0x400 && Api.MakeContext && Api.Apply && Api.AddHit &&
                     ContextSize > 0 && ContextSize <= 0x40 && Api.AddHitContext.Size == ContextSize && Api.ApplyContext.Size == ContextSize &&
                     Api.AddHitResult.Size == Api.HitSize && Api.ApplyClass.Size == sizeof(void*) && Api.ApplyTarget.Size == sizeof(void*) &&
                     Api.ApplyLevel.Size == sizeof(float) && Api.MakeLevel.Size == sizeof(int32) && Api.ContainerSize > 0 &&
                     Api.ContainerEffects.Size == sizeof(TArray<UClass*>) && ParamsSize(Api.Apply) <= 0x200 && ParamsSize(Api.MakeContext) <= 0x100 &&
                     ParamsSize(Api.AddHit) <= 0x500;

        printf("[HIT] Player gun hits: %s (hit %u bytes, context %u bytes, effect container %u bytes, origin %d, critical %d, damage zone %d).\n",
               Api.bReady ? "ready" : "MISSING PIECES, AR/SMG/pistol hits won't do damage", Api.HitSize, ContextSize, Api.ContainerSize,
               Api.SetOrigin != nullptr, Api.SetCritical != nullptr, Api.DamageZone != nullptr);
        fflush(stdout);
        return Api.bReady;
    }

    static AActor* AsActor(const UObject* Object)
    {
        if (!Object)
            return nullptr;
        if (Object->IsA(AActor::StaticClass()))
            return (AActor*)Object;
        if (Object->IsA(UActorComponent::StaticClass()))
        {
            auto Owner = ((UActorComponent*)Object)->GetOwner();
            return Owner && Owner->IsA(AActor::StaticClass()) ? (AActor*)Owner : nullptr;
        }
        return nullptr;
    }

    static AActor* WeakActorAt(const uint8* Memory, uint32 MemorySize, const FField& Where)
    {
        if (!Where.Fits(MemorySize) || Where.Size != sizeof(FWeakObjectPtr))
            return nullptr;
        FWeakObjectPtr Weak{};
        memcpy(&Weak, Memory + Where.Offset, sizeof(Weak));
        return AsActor(Weak.Get());
    }

    static AActor* HitActor(const uint8* Hit)
    {
        if (Api.HitHandle.Fits(Api.HitSize) && Api.HandleObject.Size == sizeof(FWeakObjectPtr) && Api.HandleObject.Fits(Api.HitHandle.Size))
            if (auto Actor = WeakActorAt(Hit + Api.HitHandle.Offset, Api.HitHandle.Size, Api.HandleObject))
                return Actor;
        if (auto Actor = WeakActorAt(Hit, Api.HitSize, Api.HitActor))
            return Actor;
        return WeakActorAt(Hit, Api.HitSize, Api.HitComponent);
    }

    static bool Step(FFrame& Frame, void* Out, uint32 Size)
    {
        if (!Frame.Code && !*(const UField**)(__int64(&Frame) + Offsets::FFrame_PropertyChainForCompiledIn))
            return false;
        void* Source = Frame.StepCompiledInRefInternal(Out);
        if (!Source)
            return false;
        if (Source != Out)
            memcpy(Out, Source, Size);
        return true;
    }

    static const UObject* FindAbility(UAbilitySystemComponent* AbilitySystem, int32 Handle, int32& OutLevel)
    {
        OutLevel = 1;
        if (!AbilitySystem || Handle <= 0 || !AbilitySystem->HasActivatableAbilities())
            return nullptr;

        auto& Items = AbilitySystem->ActivatableAbilities.Items;
        for (int i = 0; i < Items.Num(); i++)
        {
            auto& Spec = Items.Get(i, FGameplayAbilitySpec::Size());
            if (Spec.Handle.Handle != Handle)
                continue;
            if (FGameplayAbilitySpec::HasLevel() && Spec.Level > 0 && Spec.Level < 1000)
                OutLevel = Spec.Level;
            return Spec.Ability;
        }
        return nullptr;
    }

    static int CollectEffects(const UObject* Ability, UClass** Out, int Max)
    {
        auto Prop = Ability ? Ability->GetProperty("EffectContainers") : nullptr;
        if (!Prop)
            return 0;

        auto Offset = GetFromOffset<uint32>(Prop, Offsets::Offset_Internal);
        auto ElementSize = GetFromOffset<uint32>(Prop, Offsets::ElementSize);
        auto ArrayDim = GetFromOffset<int32>(Prop, Offsets::ElementSize - sizeof(int32));
        if (ElementSize != Api.ContainerSize || ArrayDim <= 0 || ArrayDim > 16 || !Api.ContainerEffects.Fits(ElementSize))
        {
            static bool bWarned = false;
            if (!bWarned)
            {
                bWarned = true;
                printf("[HIT] A gun ability's EffectContainers don't look like this build's (element %u bytes, %d of them, expected %u bytes).\n", ElementSize, ArrayDim, Api.ContainerSize);
                fflush(stdout);
            }
            return 0;
        }

        int Count = 0;
        for (int Container = 0; Container < ArrayDim; Container++)
        {
            auto& Classes = *(TArray<UClass*>*)((uint8*)Ability + Offset + (size_t)Container * ElementSize + Api.ContainerEffects.Offset);
            if (Classes.Num() < 0 || Classes.Num() > 32 || (Classes.Num() > 0 && !Classes.Data))
                continue;

            for (int i = 0; i < Classes.Num() && Count < Max; i++)
            {
                auto Class = Classes.Data[i];
                if (!Class || !Class->IsA(UClass::StaticClass()))
                    continue;

                bool bAlready = false;
                for (int j = 0; j < Count; j++)
                    if (Out[j] == Class)
                        bAlready = true;
                if (!bAlready)
                    Out[Count++] = Class;
            }
        }
        return Count;
    }

    static bool WeaponBelongsTo(AFortPlayerPawnAthena* Pawn, AActor* Weapon)
    {
        if (Pawn->CurrentWeapon == Weapon)
            return true;
        if (!Pawn->HasCurrentWeaponList())
            return false;
        auto& Weapons = Pawn->CurrentWeaponList;
        for (int i = 0; i < Weapons.Num() && i < 64; i++)
            if (Weapons.Data[i] == Weapon)
                return true;
        return false;
    }

    static bool WithinFireRate(AFortWeapon* Weapon, double Time)
    {
        static std::unordered_map<void*, std::pair<double, int>> Windows;
        if (Windows.size() > 256)
            Windows.clear();

        float FireRate = 10.f;
        int Pellets = 1;
        if (auto Stats = AFortInventory::GetStats(Weapon->WeaponData))
        {
            if (FFortRangedWeaponStats::HasFiringRate() && Stats->FiringRate > 0.1f)
                FireRate = Stats->FiringRate;
            if (FFortRangedWeaponStats::HasBulletsPerCartridge() && Stats->BulletsPerCartridge > 1)
                Pellets = Stats->BulletsPerCartridge;
        }

        auto& Window = Windows[(void*)Weapon];
        if (Time - Window.first > 1.0)
        {
            Window.first = Time;
            Window.second = 0;
        }
        return ++Window.second <= (int)(FireRate * Pellets * 1.5f) + 3;
    }

    static int Reject(const char* Why)
    {
        static int Rejected = 0;
        if (++Rejected <= 10 || (Rejected % 200) == 0)
        {
            printf("[HIT] A player's gun hit wasn't applied: %s (%d so far).\n", Why, Rejected);
            fflush(stdout);
        }
        return 0;
    }

    static int ApplyHit(AFortWeapon* Weapon, AFortPlayerPawnAthena* Shooter, AFortPlayerControllerAthena* ShooterController, uint8* Hit, const FVector& Origin, bool bHasOrigin,
                        AFortPlayerPawnAthena*& OutTarget)
    {
        OutTarget = nullptr;

        auto Target = HitActor(Hit);
        if (!Target || !Target->IsA<AFortPlayerPawnAthena>())
            return -1;

        auto TargetPawn = (AFortPlayerPawnAthena*)Target;
        OutTarget = TargetPawn;
        if (TargetPawn == Shooter)
            return Reject("hit themselves");
        if (TargetPawn->bActorIsBeingDestroyed || TargetPawn->GetHealth() <= 0.f)
            return Reject("target already dead");
        if (TargetPawn->HasbCanBeDamaged() && !TargetPawn->bCanBeDamaged)
            return Reject("target can't be damaged right now");
        if (ShooterController->IsInAircraft())
            return Reject("shooter is on the bus");

        auto ShooterState = (AFortPlayerStateAthena*)ShooterController->PlayerState;
        auto TargetState = (AFortPlayerStateAthena*)TargetPawn->PlayerState;
        if (!ShooterState || !TargetState)
            return Reject("no player state");
        if (ShooterState->TeamIndex == TargetState->TeamIndex)
            return Reject("same team");

        FVector Impact{};
        if (!ReadValue(Hit, Api.HitSize, Api.HitImpactPoint, Impact) && !ReadValue(Hit, Api.HitSize, Api.HitLocation, Impact))
            return Reject("no impact point");

        auto TargetLocation = TargetPawn->K2_GetActorLocation();
        auto Offset = Impact - TargetLocation;
        double Horizontal = sqrt(Offset.X * Offset.X + Offset.Y * Offset.Y);
        if (Horizontal > 300.0 || fabs(Offset.Z) > 350.0)
            return Reject("impact isn't on the target");
        if ((Impact - Shooter->K2_GetActorLocation()).Magnitude() > 100000.0)
            return Reject("too far away");
        if (!WithinFireRate(Weapon, UGameplayStatics::GetTimeSeconds(UWorld::GetWorld())))
            return Reject("more hits than the gun can fire");

        auto ShooterAbilitySystem = ShooterState->AbilitySystemComponent;
        auto TargetAbilitySystem = TargetState->AbilitySystemComponent;
        if (!ShooterAbilitySystem || !TargetAbilitySystem)
            return Reject("no ability system");

        int32 Level = 1;
        auto Ability = FindAbility(ShooterAbilitySystem, Weapon->HasPrimaryAbilitySpecHandle() ? Weapon->PrimaryAbilitySpecHandle.Handle : 0, Level);
        UClass* Effects[16]{};
        int EffectCount = CollectEffects(Ability, Effects, 16);
        if (!EffectCount && Weapon->HasImpactAbilitySpecHandle())
        {
            Ability = FindAbility(ShooterAbilitySystem, Weapon->ImpactAbilitySpecHandle.Handle, Level);
            EffectCount = CollectEffects(Ability, Effects, 16);
        }
        if (!EffectCount)
            return Reject("the gun's ability has no damage effects");

        bool bCritical = false;
        if (Api.DamageZone && Api.DamageZoneHit.Size == Api.HitSize && Api.DamageZoneReturn.Size == sizeof(uint8) && ParamsSize(Api.DamageZone) <= 0x500)
        {
            alignas(16) uint8 ZoneParams[0x500]{};
            auto ZoneSize = ParamsSize(Api.DamageZone);
            if (Write(ZoneParams, ZoneSize, Api.DamageZoneHit, Hit, Api.HitSize))
            {
                TargetPawn->ProcessEvent(Api.DamageZone, ZoneParams);
                uint8 Zone = 0xFF;
                if (ReadValue(ZoneParams, ZoneSize, Api.DamageZoneReturn, Zone))
                    bCritical = Zone == 2;
            }
        }

        alignas(16) uint8 MakeParams[0x100]{};
        auto MakeSize = ParamsSize(Api.MakeContext);
        UObject* Source = Weapon;
        AActor* Causer = Weapon;
        if (!WriteValue(MakeParams, MakeSize, Api.MakeSource, Source) || !WriteValue(MakeParams, MakeSize, Api.MakeCauser, Causer) ||
            !WriteValue(MakeParams, MakeSize, Api.MakeDamageSource, Causer) || !WriteValue(MakeParams, MakeSize, Api.MakeLevel, Level))
            return Reject("couldn't build the effect context");
        ShooterAbilitySystem->ProcessEvent(Api.MakeContext, MakeParams);
        const uint8* Context = MakeParams + Api.MakeReturn.Offset;
        const uint32 ContextSize = Api.MakeReturn.Size;

        {
            alignas(16) uint8 AddParams[0x500]{};
            auto AddSize = ParamsSize(Api.AddHit);
            bool bReset = true;
            if (!Write(AddParams, AddSize, Api.AddHitContext, Context, ContextSize) || !Write(AddParams, AddSize, Api.AddHitResult, Hit, Api.HitSize))
                return Reject("couldn't add the hit to the context");
            WriteValue(AddParams, AddSize, Api.AddHitReset, bReset);
            Api.Library->ProcessEvent(Api.AddHit, AddParams);
        }

        if (Api.SetOrigin && bHasOrigin)
        {
            alignas(16) uint8 OriginParams[0x80]{};
            auto OriginSize = ParamsSize(Api.SetOrigin);
            if (OriginSize <= sizeof(OriginParams) && Write(OriginParams, OriginSize, Api.SetOriginContext, Context, ContextSize) &&
                Write(OriginParams, OriginSize, Api.SetOriginValue, &Origin, (uint32)sizeof(FVector)))
                Api.Library->ProcessEvent(Api.SetOrigin, OriginParams);
        }

        if (Api.SetCritical)
        {
            alignas(16) uint8 CriticalParams[0x80]{};
            auto CriticalSize = ParamsSize(Api.SetCritical);
            if (CriticalSize <= sizeof(CriticalParams) && Write(CriticalParams, CriticalSize, Api.SetCriticalContext, Context, ContextSize) &&
                WriteValue(CriticalParams, CriticalSize, Api.SetCriticalValue, bCritical))
                Api.FortLibrary->ProcessEvent(Api.SetCritical, CriticalParams);
        }

        int Applied = 0;
        auto ApplySize = ParamsSize(Api.Apply);
        float EffectLevel = (float)Level;
        for (int i = 0; i < EffectCount; i++)
        {
            alignas(16) uint8 ApplyParams[0x200]{};
            UClass* EffectClass = Effects[i];
            if (!WriteValue(ApplyParams, ApplySize, Api.ApplyClass, EffectClass) || !WriteValue(ApplyParams, ApplySize, Api.ApplyTarget, TargetAbilitySystem) ||
                !Write(ApplyParams, ApplySize, Api.ApplyContext, Context, ContextSize) || !WriteValue(ApplyParams, ApplySize, Api.ApplyLevel, EffectLevel))
                continue;
            WriteValue(ApplyParams, ApplySize, Api.ApplySource, Source);
            WriteValue(ApplyParams, ApplySize, Api.ApplyCauser, Causer);
            WriteValue(ApplyParams, ApplySize, Api.ApplyDamageSource, Causer);

            ShooterAbilitySystem->ProcessEvent(Api.Apply, ApplyParams);

            FActiveGameplayEffectHandle Result{};
            if (ReadValue(ApplyParams, ApplySize, Api.ApplyReturn, Result) && Result.bPassedFiltersAndWasExecuted)
                Applied++;
        }

        if (!Applied)
            return Reject("the game turned down the gun's damage effects");

        static int Reported = 0;
        if (++Reported <= 10)
        {
            printf("[HIT] %s hit %s%s: %d of %d effects applied, target now %.0f health %.0f shield.\n", Weapon->WeaponData ? Weapon->WeaponData->Name.ToString().c_str() : "a gun",
                   TargetPawn->Class->Name.ToString().c_str(), bCritical ? " in the head" : "", Applied, EffectCount, TargetPawn->GetHealth(), TargetPawn->GetShield());
            fflush(stdout);
        }
        return Applied;
    }
}

static void (*ServerNotifyPawnHitOG)(UObject*, FFrame&, void*) = nullptr;

static void ServerNotifyPawnHit(UObject* Context, FFrame& Stack, void* Result)
{
    auto CallOriginal = [&]()
    {
        if (ServerNotifyPawnHitOG)
            ServerNotifyPawnHitOG(Context, Stack, Result);
    };

    static bool bInside = false;
    auto Weapon = Context ? Context->Cast<AFortWeapon>() : nullptr;
    if (bInside || Stack.Code || !Weapon || !PlayerHits::Resolve(Stack.GetCurrentNativeFunction()))
        return CallOriginal();

    auto Shooter = Weapon->Instigator && Weapon->Instigator->IsA<AFortPlayerPawnAthena>() ? (AFortPlayerPawnAthena*)Weapon->Instigator : nullptr;
    if (!Shooter && Weapon->Owner && Weapon->Owner->IsA<AFortPlayerPawnAthena>())
        Shooter = (AFortPlayerPawnAthena*)Weapon->Owner;
    auto ShooterController = Shooter && Shooter->Controller && Shooter->Controller->IsA<AFortPlayerControllerAthena>() ? (AFortPlayerControllerAthena*)Shooter->Controller : nullptr;
    if (!ShooterController || !PlayerHits::WeaponBelongsTo(Shooter, Weapon))
        return CallOriginal();

    alignas(16) uint8 FrameCopy[0x100]{};
    uint32 Need = (uint32)sizeof(FFrame);
    Need = (std::max)(Need, Offsets::FFrame_PropertyChainForCompiledIn + (uint32)sizeof(void*));
    Need = (std::max)(Need, Offsets::FFrame_CurrentNativeFunction + (uint32)sizeof(void*));
    if (Need > sizeof(FrameCopy))
        return CallOriginal();
    memcpy(FrameCopy, &Stack, Need);
    auto& Copy = *(FFrame*)FrameCopy;

    alignas(16) uint8 Hit[0x400]{};
    FVector Origin{};
    if (!PlayerHits::Step(Copy, Hit, PlayerHits::Api.HitSize))
        return CallOriginal();
    bool bHasOrigin = PlayerHits::Api.NotifyOrigin.Size == sizeof(FVector) && PlayerHits::Step(Copy, &Origin, (uint32)sizeof(FVector));

    bInside = true;
    AFortPlayerPawnAthena* Target = nullptr;
    int Applied = PlayerHits::ApplyHit(Weapon, Shooter, ShooterController, Hit, Origin, bHasOrigin, Target);
    bInside = false;

    if (Applied < 0)
        return CallOriginal();

    if (Applied > 0 && Target)
        Bots::NoteAttacker(Target, ShooterController);

    Stack.IncrementCode();
}

// ----- FortMME / mantling (ported from Boron, github.com/Ducki67/Boron) -----

static const char* ClamberingStateName(int State)
{
    static const char* Names[] = { "Inactive", "Targeting", "Ledge_Moving", "Ledge_Failed", "Window_Moving", "Window_Failed" };
    return State >= 0 && State < 6 ? Names[State] : "?";
}

void UClamberingComponent::ServerStartClambering(UObject* Context, FFrame& Stack)
{
    auto Comp = (UClamberingComponent*)Context;
    int StateBefore = Comp->HasReplicatedClamberingState() ? (int)Comp->ReplicatedClamberingState : -1;

    ServerStartClamberingOG(Context, Stack);

    auto WarpPawn = Comp->Outer && Comp->Outer->IsA<AFortPlayerPawnAthena>() ? (AFortPlayerPawnAthena*)Comp->Outer : nullptr;
    bool bWarped = false;
    if (WarpPawn && WarpPawn->GetFunction("OnRep_SynchedActionWarpPointInfo"))
    {
        WarpPawn->OnRep_SynchedActionWarpPointInfo();
        bWarped = true;
    }

    static int Logged = 0;
    if (++Logged <= 12 || (Logged % 50) == 0)
    {
        int StateAfter = Comp->HasReplicatedClamberingState() ? (int)Comp->ReplicatedClamberingState : -1;
        int LocalState = Comp->HasLocalClamberingState() ? (int)Comp->LocalClamberingState : -1;
        printf("[Clamber] A player started a mantle (#%d): server state %s -> %s (local %s), warp point run on the server: %s.\n", Logged,
               ClamberingStateName(StateBefore), ClamberingStateName(StateAfter), ClamberingStateName(LocalState), bWarped ? "yes" : "no (pawn or OnRep missing)");
        fflush(stdout);
    }
}

void UClamberingComponent::NetMulticast_ClamberingLedgeFailed(UObject* Context, FFrame& Stack)
{
    static const char* Reasons[] = { "None",
                                     "Unknown",
                                     "DebugForced",
                                     "OwnerDied",
                                     "OwnerDBNO",
                                     "OwnerLaunched",
                                     "SynchedActionNotStarted",
                                     "OwnerTeleported",
                                     "Ledge_PlayerTooFar",
                                     "Ledge_TargetLocationInvalid",
                                     "Ledge_TargetActorInvalid",
                                     "Ledge_TargetActorDestroyed",
                                     "Ledge_BlockerEncountered" };

    int Reason = !Stack.Code && Stack.Locals ? (int)*(uint8*)(__int64(Stack.Locals) + 0x0) : -1;
    int State = !Stack.Code && Stack.Locals ? (int)*(uint8*)(__int64(Stack.Locals) + 0x1) : -1;

    static int Logged = 0;
    if (++Logged <= 24 || (Logged % 50) == 0)
    {
        printf("[Clamber] A mantle FAILED on the server (#%d): reason %d (%s), state %d (%s).\n", Logged, Reason, Reason >= 0 && Reason < 13 ? Reasons[Reason] : "?",
               State, ClamberingStateName(State));
        fflush(stdout);
    }

    return NetMulticast_ClamberingLedgeFailedOG(Context, Stack);
}

void UClamberingComponent::Configure(AActor* Pawn)
{
    if (VersionInfo.EngineVersion < 5.4 || !Pawn)
        return;

    static int Logged = 0;
    bool bLog = Logged++ < 4;

    auto Cls = StaticClass();
    auto Comp = Cls ? (UClamberingComponent*)Pawn->GetComponentByClass((UClass*)Cls) : nullptr;

    if (!Comp)
    {
        if (bLog)
        {
            printf("[Clamber] The player's pawn has no clambering component (class %s), nothing to set up.\n", Cls ? "found" : "missing on this build");
            fflush(stdout);
        }
        return;
    }

    float MaxDistance = Comp->HasServerValidatePlayerMaxDistance() ? Comp->ServerValidatePlayerMaxDistance.Evaluate() : -1.f;

    if (Comp->HasServerValidatePlayerMaxDistance() && MaxDistance < 500.f)
    {
        Comp->ServerValidatePlayerMaxDistance.Curve.CurveTable = nullptr;
        Comp->ServerValidatePlayerMaxDistance.Value = 5000.f;
    }

    if (bLog)
    {
        auto MME = Comp->HasMovementModeExtension() ? (UObject*)Comp->MovementModeExtension : nullptr;
        printf("[Clamber] Player pawn's clambering: enabled %.2f, indicator %.2f, max distance %.0f -> %.0f, fail delay %.2f, synched action fail delay %.2f, "
               "MME class %s, IsClamberingEnabled %d, auto clamber %d, targeting while walking %d / swimming %d, shows indicator %d (-1 = not on this build).\n",
               Comp->HasClamberingEnabled() ? Comp->ClamberingEnabled.Evaluate() : -1.f, Comp->HasClamberIndicatorEnabled() ? Comp->ClamberIndicatorEnabled.Evaluate() : -1.f,
               MaxDistance, Comp->HasServerValidatePlayerMaxDistance() ? Comp->ServerValidatePlayerMaxDistance.Evaluate() : -1.f,
               Comp->HasServerFailDelay() ? Comp->ServerFailDelay.Evaluate() : -1.f, Comp->HasSynchedActionFailDelay() ? Comp->SynchedActionFailDelay.Evaluate() : -1.f,
               MME ? MME->Name.ToString().c_str() : "none", Comp->GetFunction("IsClamberingEnabled") ? (int)Comp->IsClamberingEnabled() : -1,
               Comp->GetFunction("IsAutoClamberingEnabled") ? (int)Comp->IsAutoClamberingEnabled() : -1,
               Comp->HasbPerformTargetingWhileWalking() ? (int)Comp->bPerformTargetingWhileWalking : -1,
               Comp->HasbPerformTargetingWhileSwimming() ? (int)Comp->bPerformTargetingWhileSwimming : -1,
               Comp->GetFunction("ShouldShowClamberIndicator") ? (int)Comp->ShouldShowClamberIndicator() : -1);
        fflush(stdout);
    }
}

void UClamberingComponent::PostLoadHook()
{
    if (VersionInfo.EngineVersion < 5.4)
        return;

    auto Default = GetDefaultObj();

    if (!Default)
    {
        printf("[Clamber] This build has no ClamberingComponent class, the mantling hooks are off.\n");
        fflush(stdout);
        return;
    }

    auto StartFn = Default->GetFunction("ServerStartClambering");
    auto FailFn = Default->GetFunction("NetMulticast_ClamberingLedgeFailed");

    printf("[Clamber] Mantling hooks: ServerStartClambering %s, NetMulticast_ClamberingLedgeFailed %s.\n", StartFn ? "hooked" : "MISSING", FailFn ? "hooked" : "MISSING");
    fflush(stdout);

    if (StartFn)
        Hooking::ExecHook(StartFn, ServerStartClambering, ServerStartClamberingOG);

    if (FailFn)
        Hooking::ExecHook(FailFn, NetMulticast_ClamberingLedgeFailed, NetMulticast_ClamberingLedgeFailedOG);
}

struct FLaunchedPawn
{
    FWeakObjectPtr Pawn;
    uint64_t Time;
};

static std::vector<FLaunchedPawn> LaunchedPawns;

static void SetServerCorrections(AFortPlayerPawnAthena* Pawn, bool bClientAuthoritative)
{
    if (!Pawn->HasCharacterMovement() || !Pawn->CharacterMovement)
        return;

    auto MoveComp = Pawn->CharacterMovement;

    if (MoveComp->HasbIgnoreClientMovementErrorChecksAndCorrection())
        MoveComp->bIgnoreClientMovementErrorChecksAndCorrection = bClientAuthoritative;

    if (MoveComp->HasbServerAcceptClientAuthoritativePosition())
        MoveComp->bServerAcceptClientAuthoritativePosition = bClientAuthoritative;
}

void AFortPlayerPawnAthena::LaunchCharacterExec(UObject* Context, FFrame& Stack)
{
    LaunchCharacterExecOG(Context, Stack);

    if (VersionInfo.EngineVersion < 5.4 || !Context || !Context->IsA<AFortPlayerPawnAthena>())
        return;

    auto Pawn = (AFortPlayerPawnAthena*)Context;
    if (!Pawn->Controller || !Pawn->Controller->IsA<AFortPlayerControllerAthena>())
        return;

    SetServerCorrections(Pawn, false);

    bool bFound = false;
    for (auto& Launched : LaunchedPawns)
        if (Launched.Pawn.Get() == (const UObject*)Pawn)
        {
            Launched.Time = GetTickCount64();
            bFound = true;
        }

    if (!bFound)
        LaunchedPawns.push_back({ FWeakObjectPtr(Pawn), GetTickCount64() });

    static int Logged = 0;
    if (++Logged <= 10)
    {
        printf("[MME] A player was launched, the server's movement corrections are back on until they land.\n");
        fflush(stdout);
    }
}

void AFortPlayerPawnAthena::TickLaunchCorrections()
{
    if (LaunchedPawns.empty())
        return;

    auto Now = GetTickCount64();

    for (size_t i = 0; i < LaunchedPawns.size();)
    {
        auto& Launched = LaunchedPawns[i];
        auto Pawn = (AFortPlayerPawnAthena*)Launched.Pawn.Get();

        if (!Pawn)
        {
            LaunchedPawns.erase(LaunchedPawns.begin() + i);
            continue;
        }

        auto Elapsed = Now - Launched.Time;
        auto MoveComp = Pawn->HasCharacterMovement() ? Pawn->CharacterMovement : nullptr;
        int Mode = MoveComp && MoveComp->HasMovementMode() ? (int)MoveComp->MovementMode : -1;
        bool bLanded = Elapsed > 500 && Mode == 1 ;

        if (bLanded || Elapsed > 10000)
        {
            SetServerCorrections(Pawn, true);

            static int Logged = 0;
            if (++Logged <= 10)
            {
                printf("[MME] A launched player %s after %llums, the server takes their own position again (movement mode %d).\n", bLanded ? "landed" : "was still in the air",
                       (unsigned long long)Elapsed, Mode);
                fflush(stdout);
            }

            LaunchedPawns.erase(LaunchedPawns.begin() + i);
            continue;
        }

        i++;
    }
}

void AFortPlayerPawnAthena::PostLoadHook()
{
    OnRep_ZiplineState = FindOnRep_ZiplineState();
    SetPickupTarget_ = FindSetPickupTarget();

    auto ServerHandlePickupInfoFn = GetDefaultObj()->GetFunction("ServerHandlePickupInfo");

    if (ServerHandlePickupInfoFn)
        Hooking::ExecHook(ServerHandlePickupInfoFn, ServerHandlePickupInfo);
    else
    {
        Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerHandlePickup"), ServerHandlePickup_);
        Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerHandlePickupWithRequestedSwap"), ServerHandlePickupWithRequestedSwap);
    }

    Hooking::Hook(FindFinishedTargetSpline(), FinishedTargetSpline, FinishedTargetSplineOG);
    Hooking::ExecHook(GetDefaultObj()->GetFunction("OnCapsuleBeginOverlap"), OnCapsuleBeginOverlap_, OnCapsuleBeginOverlap_OG);

    if (auto RangedWeaponClass = AFortWeaponRanged::StaticClass())
        if (auto RangedWeaponDefault = RangedWeaponClass->GetDefaultObj())
            Hooking::ExecHook(RangedWeaponDefault->GetFunction("ServerNotifyPawnHit"), ServerNotifyPawnHit, ServerNotifyPawnHitOG);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerSendZiplineState"), ServerSendZiplineState);
    Hooking::ExecHook(GetDefaultObj()->GetFunction("MovingEmoteStopped"), MovingEmoteStopped);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerOnExitVehicle"), ServerOnExitVehicle_, ServerOnExitVehicle_OG);

    Hooking::ExecHook(GetDefaultObj()->GetFunction("EmoteStopped"), EmoteStopped_, EmoteStopped_OG);

    auto EndSkydivingFn = GetDefaultObj()->GetFunction("EndSkydiving");

    if (EndSkydivingFn)
        Hooking::Hook<AFortPlayerPawnAthena>(EndSkydivingFn->GetVTableIndex(), EndSkydiving, EndSkydivingOG);

    if (VersionInfo.EngineVersion >= 5.4)
    {
        auto LaunchCharacterFn = GetDefaultObj()->GetFunction("LaunchCharacter");
        printf("[MME] LaunchCharacter hook: %s.\n", LaunchCharacterFn ? "hooked" : "MISSING");
        fflush(stdout);
        if (LaunchCharacterFn)
            Hooking::ExecHook(LaunchCharacterFn, LaunchCharacterExec, LaunchCharacterExecOG);
    }

    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerReviveFromDBNO"), ServerReviveFromDBNO);
    Hooking::ExecHook(GetDefaultObj()->GetFunction("ServerThrowCarriedPlayer"), ServerThrowCarriedPlayer_, ServerThrowCarriedPlayer_OG);

    if (VersionInfo.FortniteVersion >= 18)
    {
        auto SetIsInsideSafeZoneBase = Memcury::Scanner::FindPattern("74 ? 33 D2 48 8B ? E8 ? ? ? ? 48 8B ? 41 8A ? 48 8B ? FF 90");

        if (!SetIsInsideSafeZoneBase.IsValid())
            SetIsInsideSafeZoneBase = Memcury::Scanner::FindPattern("74 ? 33 D2 48 8B ? E8 ? ? ? ? 48 8B ? B2 01 48 8B ? FF 90");

        if (!SetIsInsideSafeZoneBase.IsValid())
            SetIsInsideSafeZoneBase = Memcury::Scanner::FindPattern("0F 84 ? ? ? ? 33 D2 48 8B ? E8 ? ? ? ? 48 8B ? B2 01 48 8B ? FF 90");

        if (SetIsInsideSafeZoneBase.IsValid())
        {
            auto SetIsInsideSafeZoneVftPtr = SetIsInsideSafeZoneBase.ScanFor({ 0xFF, 0x90 }).AbsoluteOffset(2).Get();

            Hooking::Hook<AFortPlayerPawnAthena>(*(uint32_t*)SetIsInsideSafeZoneVftPtr / 8, SetIsInsideSafeZone);
            Hooking::Hook<AFortPlayerPawnAthena>(*(uint32_t*)SetIsInsideSafeZoneVftPtr / 8 + 1, UpdateOutsideSafeZone);
        }
    }
}
