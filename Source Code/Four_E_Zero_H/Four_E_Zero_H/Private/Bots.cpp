#include "pch.h"
#include "../Public/Bots.h"
#include "../../FortniteGame/Public/BattleRoyaleGamePhaseLogic.h"
#include "../../FortniteGame/Public/FortGameMode.h"
#include "../../FortniteGame/Public/FortInventory.h"
#include "../../FortniteGame/Public/FortKismetLibrary.h"
#include "../../FortniteGame/Public/FortPlayerControllerAthena.h"
#include "../../FortniteGame/Public/FortWeapon.h"
#include "../../FortniteGame/Public/FortLootPackage.h"
#include "../../FortniteGame/Public/BuildingContainer.h"
#include "../Public/Configuration.h"
#include "../Public/hde64.hpp"
#include "../Plugins/CrashReporter/Public/CrashReporter.h"
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <cstdarg>
#include <share.h>
#include <objbase.h>

extern uint64_t NotifyGameMemberAdded_;
extern uint64_t InitializePlayerGameplayAbilities_;

static void BotLog(const char* Format, ...)
{
    static FILE* File = nullptr;
    static bool bOpened = false;
    if (!bOpened)
    {
        bOpened = true;
        HMODULE Module = nullptr;
        wchar_t Path[MAX_PATH]{};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&BotLog, &Module) &&
            GetModuleFileNameW(Module, Path, MAX_PATH))
        {
            std::wstring LogPath = Path;
            LogPath = LogPath.substr(0, LogPath.find_last_of(L"\\/") + 1) + L"4e0h-Bots.log";
            File = _wfsopen(LogPath.c_str(), L"w", _SH_DENYNO);
            if (!File) { CreateDirectoryW(L"FortniteGame\\Content\\Gameserver Logs", nullptr); File = _wfsopen(L"FortniteGame\\Content\\Gameserver Logs\\4e0h-Bots.log", L"w", _SH_DENYNO); }
            if (!File) File = _wfsopen(L"4e0h-Bots.log", L"w", _SH_DENYNO);
        }
    }

    char Buffer[1024];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);

    printf("%s", Buffer);
    if (File)
    {
        fputs(Buffer, File);
        fflush(File);
    }
}

void Bots::Log(const char* Format, ...)
{
    char Buffer[1024];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);
    BotLog("%s", Buffer);
}

class ABotController : public AActor
{
public:
    DEFINE_PROP(Pawn, AActor*);
    DEFINE_PROP(PlayerState, AFortPlayerStateAthena*);
    DEFINE_PROP(BrainComponent, UObject*);
    DEFINE_PROP(Blackboard, UObject*);
    DEFINE_PROP(BehaviorTree, UObject*);

    static inline int32 Inventory__Offset = -2;
    static inline bool Inventory__bInterface = false;

    int32 GetInventoryOffset() const
    {
        if (Inventory__Offset == -2)
        {
            Inventory__Offset = -1;
            if (auto Prop = GetProperty("Inventory", 0x1000 ))
            {
                Inventory__Offset = (int32)GetFromOffset<uint32>(Prop, Offsets::Offset_Internal);
                Inventory__bInterface = true;
            }
            else if (auto Prop = GetProperty("Inventory", 0x10000 ))
                Inventory__Offset = (int32)GetFromOffset<uint32>(Prop, Offsets::Offset_Internal);

            BotLog("[Bots] Bot controller inventory field: %s.\n", Inventory__Offset == -1 ? "MISSING on this build" : Inventory__bInterface ? "interface (29.x+)" : "object pointer");
        }
        return Inventory__Offset;
    }

    bool HasInventory() const { return GetInventoryOffset() != -1; }

    AFortInventory* GetInventory() const
    {
        auto Offset = GetInventoryOffset();
        return Offset == -1 ? nullptr : *(AFortInventory**)(__int64(this) + Offset);
    }

    void SetInventory(AFortInventory* NewInventory) const
    {
        auto Offset = GetInventoryOffset();
        if (Offset == -1)
            return;

        if (Inventory__bInterface)
        {
            auto& ScriptInterface = *(TScriptInterface<IFortInventoryInterface>*)(__int64(this) + Offset);
            ScriptInterface.ObjectPointer = NewInventory;
            ScriptInterface.InterfacePointer = NewInventory ? NewInventory->GetInterface(IFortInventoryInterface::StaticClass()) : nullptr;
        }
        else
            *(AFortInventory**)(__int64(this) + Offset) = NewInventory;
    }

    __declspec(property(get = GetInventory, put = SetInventory)) AFortInventory* Inventory;

    DEFINE_FUNC(RunBehaviorTree, bool);
    DEFINE_FUNC(SetControlRotation, void);
    DEFINE_FUNC(LineOfSightTo, bool);
    DEFINE_FUNC(IsMoveInputIgnored, bool);
    DEFINE_FUNC(ResetIgnoreMoveInput, void);
    DEFINE_FUNC(ThankBusDriver, void);
};

class UBotBrain : public UObject
{
public:
    DEFINE_FUNC(StopLogic, void);
    DEFINE_FUNC(RestartLogic, void);
    DEFINE_FUNC(IsRunning, bool);
};

class UBotBlackboard : public UObject
{
public:
    DEFINE_FUNC(SetValueAsBool, void);
    DEFINE_FUNC(SetValueAsEnum, void);
    DEFINE_FUNC(SetValueAsVector, void);
};

class UBotMovement : public UCharacterMovementComponent
{
public:
    DEFINE_FUNC(DisableMovement, void);
    DEFINE_FUNC(IsMovingOnGround, bool);
};

class ABotPawn : public AFortPlayerPawnAthena
{
public:
    DEFINE_BITFIELD_PROP(bIsDying);

    DEFINE_FUNC(AddMovementInput, void);
    DEFINE_FUNC(IsDead, bool);
    DEFINE_FUNC(ForceKill, void);
    DEFINE_FUNC(SetActorEnableCollision, void);
    DEFINE_FUNC(PawnStartFire, void);
    DEFINE_FUNC(PawnStopFire, void);
    DEFINE_FUNC(Jump, void);
};

class UBotAISpawner : public UObject
{
public:
    DEFINE_FUNC(RequestSpawn, int32);
};

class UFortAthenaAISpawnerData : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UFortAthenaAISpawnerData);

    DEFINE_STATIC_FUNC(CreateComponentListFromClass, UObject*);
};

class AFortPlayerStateAthenaBot : public AFortPlayerStateAthena
{
public:
    DEFINE_FUNC(OnRep_TeamIndex, void);
};

enum class EBotState : uint8
{
    Playing,
    OnBus,
    Skydiving,
    Dead
};

struct FBot
{
    FWeakObjectPtr Controller;
    FWeakObjectPtr Pawn;
    FWeakObjectPtr PlayerState;
    EBotState State = EBotState::Playing;

    double JumpTime = -1;
    FVector LandingTarget;
    double SkydiveStartTime = 0;
    double LastMoveTime = 0;
    double NextStormDamageTime = 0;
    double DeathTime = 0;

    double NextDirectorTime = 0;
    FVector LastDirectorLocation;
    int32 IdleChecks = 0;
    FWeakObjectPtr LootTarget;
    double LootTargetTime = 0;
    bool bRotating = false;

    FVector MoveGoal;
    bool bHasMoveGoal = false;
    double MoveGoalTime = 0;
    FVector LastStuckLocation;
    double NextStuckCheckTime = 0;
    int32 StuckCount = 0;
    FWeakObjectPtr Target;
    double NextTargetSearchTime = 0;
    bool bFiring = false;
    double NextFireToggleTime = 0;
    double StrafeUntil = 0;
    float StrafeDirection = 1.f;
    float AimErrorPitch = 0, AimErrorYaw = 0;
    double NextAimErrorTime = 0;
    double SlowTime = 0;
    double LastSkydiveZ = 0;
    double SkydiveStillTime = 0;
    FWeakObjectPtr LastAttacker;
    double NextShotTime = 0;
    double DetourUntil = 0;
    double NextNativeBrainCheck = 0;
    bool bNativeBrainRunning = false;
    bool bSeasonLoadoutGiven = false;
    bool bTriggerFaulted = false;
    double ThankTime = -1;
    double NextDamageCheck = 0;
    float LastHealth = -1.f;
    float LastShield = -1.f;
    double LastHurtTime = -100;
    double NextBuildTime = 0;
    double BuildWindowStart = -100;
    int32 BuildsThisFight = 0;
    double ReloadStartTime = -1;
    double LastTargetTime = -100;
};

static std::vector<FBot> AllBots;
static int32 RequestedBots = 0;
static int32 PendingSpawns = 0;
static int32 SpawnedBots = 0;
static bool bDeclaredWinner = false;
static uint8 LastGamePhase = 0;
static uint8 LastGamePhaseStep = 0;
static double NextScanTime = 0;
static std::mt19937 Rng((uint32_t)time(nullptr));

static float RandRange(float Min, float Max)
{
    return std::uniform_real_distribution<float>(Min, Max)(Rng);
}

static int RandInt(int Min, int Max)
{
    return std::uniform_int_distribution<int>(Min, Max)(Rng);
}

static double Now()
{
    return UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
}

static FRotator MakeRotator(double Pitch, double Yaw, double Roll = 0)
{
    FRotator Rot{};
    Rot.Pitch = Pitch;
    Rot.Yaw = Yaw;
    Rot.Roll = Roll;
    return Rot;
}

static FRotator LookAt(const FVector& From, const FVector& To)
{
    auto Dir = To - From;
    double Flat = sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y);
    return MakeRotator(atan2(Dir.Z, Flat) * 57.29577951308232, atan2(Dir.Y, Dir.X) * 57.29577951308232);
}

static double Dist2D(const FVector& A, const FVector& B)
{
    double X = A.X - B.X, Y = A.Y - B.Y;
    return sqrt(X * X + Y * Y);
}

template <typename T = AActor>
static T* GetWeak(const FWeakObjectPtr& Ptr)
{
    auto Actor = (T*)Ptr.Get();
    if (!Actor || Actor->bActorIsBeingDestroyed)
        return nullptr;
    return Actor;
}

static AFortGameMode* GetGameMode()
{
    auto World = UWorld::GetWorld();
    return World ? (AFortGameMode*)World->AuthorityGameMode : nullptr;
}

static UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GetGamePhaseLogic()
{
    if (VersionInfo.FortniteVersion < 25.20)
        return nullptr;
    return UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld());
}

static uint8 ReadByteProp(UObject* Object, const char* Name)
{
    if (!Object)
        return 0;
    auto Offset = Object->GetOffset(Name);
    return Offset != -1 ? *(uint8*)(__int64(Object) + Offset) : 0;
}

static uint8 GetGamePhase()
{
    if (auto GamePhaseLogic = GetGamePhaseLogic())
        return ReadByteProp(GamePhaseLogic, "GamePhase");

    auto GameMode = GetGameMode();
    return GameMode && GameMode->GameState ? GameMode->GameState->GamePhase : 0;
}

static uint8 GetGamePhaseStep()
{
    if (auto GamePhaseLogic = GetGamePhaseLogic())
        return ReadByteProp(GamePhaseLogic, "GamePhaseStep");

    auto GameMode = GetGameMode();
    return GameMode ? ReadByteProp(GameMode->GameState, "GamePhaseStep") : 0;
}

static AFortAthenaAircraft* GetAircraft()
{
    if (auto GamePhaseLogic = GetGamePhaseLogic())
        return GamePhaseLogic->Aircrafts_GameState.Num() > 0 ? GamePhaseLogic->Aircrafts_GameState[0].Get() : nullptr;

    auto GameState = GetGameMode()->GameState;
    if (GameState->HasAircrafts())
        return GameState->Aircrafts.Num() > 0 ? GameState->Aircrafts[0] : nullptr;
    return GameState->HasAircraft() ? GameState->Aircraft : nullptr;
}

static AFortSafeZoneIndicator* GetSafeZoneIndicator()
{
    if (auto GamePhaseLogic = GetGamePhaseLogic())
        return GamePhaseLogic->SafeZoneIndicator;
    return GetGameMode()->SafeZoneIndicator;
}

static bool IsInSafeZone(const FVector& Location)
{
    if (auto GamePhaseLogic = GetGamePhaseLogic())
    {
        bool bInZone = GamePhaseLogic->IsInCurrentSafeZone(Location, false);
        if (UFortGameStateComponent_BattleRoyaleGamePhaseLogic::IsInCurrentSafeZone__Ptr)
            return bInZone;
    }

    return GetGameMode()->IsInCurrentSafeZone(Location, false);
}

static TArray<AActor*> GetActorsOfClass(const UClass* Class)
{
    TArray<AActor*> Actors;
    if (Class)
        Utils::GetAll(Class, Actors);
    return Actors;
}

static UObject* GetObjectProp(UObject* Object, const char* Name)
{
    if (!Object)
        return nullptr;
    auto Offset = Object->GetOffset(Name);
    return Offset != -1 ? *(UObject**)(__int64(Object) + Offset) : nullptr;
}

static void SetObjectProp(UObject* Object, const char* Name, UObject* Value)
{
    if (!Object)
        return;
    auto Offset = Object->GetOffset(Name);
    if (Offset != -1)
        *(UObject**)(__int64(Object) + Offset) = Value;
}

static bool IsController(const UObject* Object)
{
    static auto ControllerClass = FindClass("Controller");
    return Object && Object->IsA(ControllerClass);
}

static AFortPlayerStateAthena* GetControllerPlayerState(AActor* Controller)
{
    return IsController(Controller) ? ((ABotController*)Controller)->PlayerState : nullptr;
}

static bool IsPawnDead(ABotPawn* Pawn)
{
    if (!Pawn || Pawn->bActorIsBeingDestroyed)
        return true;
    if (Pawn->HasbIsDying() && Pawn->bIsDying)
        return true;
    if (Pawn->IsDead())
        return true;
    return Pawn->GetHealth() <= 0.f;
}

static FName MakeName(const wchar_t* Name)
{
    return UKismetStringLibrary::Conv_StringToName(FString(Name));
}

static void SetBlackboardBool(ABotController* Controller, const wchar_t* Key, bool bValue)
{
    if (auto Blackboard = (UBotBlackboard*)Controller->Blackboard)
        Blackboard->SetValueAsBool(MakeName(Key), bValue);
}

static void SetBlackboardEnum(ABotController* Controller, const wchar_t* Key, uint8 Value)
{
    if (auto Blackboard = (UBotBlackboard*)Controller->Blackboard)
        Blackboard->SetValueAsEnum(MakeName(Key), Value);
}

static void SetBlackboardVector(ABotController* Controller, const wchar_t* Key, const FVector& Value)
{
    if (auto Blackboard = (UBotBlackboard*)Controller->Blackboard)
        Blackboard->SetValueAsVector(MakeName(Key), Value);
}

static void UpdatePhaseKeys(ABotController* Controller)
{
    SetBlackboardEnum(Controller, L"AIEvaluator_Global_GamePhase", GetGamePhase());
    SetBlackboardEnum(Controller, L"AIEvaluator_Global_GamePhaseStep", GetGamePhaseStep());

    static auto ExecutionStatus = FindEnum("EExecutionStatus");
    static auto ExecutionAllowed = ExecutionStatus ? ExecutionStatus->GetValue("ExecutionAllowed") : -1;
    if (ExecutionAllowed >= 0)
        SetBlackboardEnum(Controller, L"AIEvaluator_RangeAttack_ExecutionStatus", (uint8)ExecutionAllowed);
}

static void SetBrainActive(ABotController* Controller, bool bActive)
{
    auto Brain = (UBotBrain*)Controller->BrainComponent;
    if (!Brain)
        return;

    if (bActive)
        Brain->RestartLogic();
    else
        Brain->StopLogic(FString(L"Four_E_Zero_HBus"));
}

static void StartBehaviorTree(ABotController* Controller)
{
    auto Tree = Controller->BehaviorTree;
    if (!Tree)
    {
        static auto PhoebeTree = FindObject<UObject>(L"/Game/Athena/AI/Phoebe/BehaviorTrees/BT_Phoebe.BT_Phoebe", FindClass("BehaviorTree"));
        Tree = (UObject*)PhoebeTree;
    }

    if (Tree)
        Controller->RunBehaviorTree(Tree);
    else
        SetBrainActive(Controller, true);
}

static UObject* GetAISystem()
{
    auto World = UWorld::GetWorld();
    return World ? GetObjectProp(World, "AISystem") : nullptr;
}

static const UClass* GetPhoebeSpawnerData()
{
    static const UClass* SpawnerData = nullptr;
    static bool bSearched = false;
    if (bSearched)
        return SpawnerData;
    bSearched = true;

    static auto ClassClass = FindClass("Class");
    if (FConfiguration::BotSpawnerData && *FConfiguration::BotSpawnerData)
        SpawnerData = FindObject<UClass>(FConfiguration::BotSpawnerData, ClassClass);

    if (!SpawnerData)
        SpawnerData = FindObject<UClass>(L"/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C", ClassClass);

    if (!SpawnerData)
    {
        auto PlayerBotSpawnerDataClass = FindClass("FortAthenaPlayerBotSpawnerData");
        if (!PlayerBotSpawnerDataClass)
            PlayerBotSpawnerDataClass = FindClass("FortAthenaAIBotSpawnerData");

        for (int i = 0; PlayerBotSpawnerDataClass && i < TUObjectArray::Num(); i++)
        {
            auto Object = TUObjectArray::GetObjectByIndex(i);
            if (!Object || !Object->IsA(ClassClass) || Object->Name.ToString().find("Phoebe") == std::string::npos)
                continue;

            auto DefaultObject = ((const UClass*)Object)->GetDefaultObj();
            if (DefaultObject && DefaultObject->IsA(PlayerBotSpawnerDataClass))
            {
                SpawnerData = (const UClass*)Object;
                break;
            }
        }
    }

    if (SpawnerData)
        BotLog("[Bots] Using spawner data %s\n", SpawnerData->Name.ToString().c_str());
    else
        BotLog("[Bots] Couldn't find the Phoebe spawner data, bots can't spawn!\n");

    return SpawnerData;
}

static void SetupBotManager(AFortGameMode* GameMode)
{
    if (!GameMode->HasServerBotManager())
        return;

    static auto ClassClass = FindClass("Class");
    static auto PhoebeManagerClass = FindObject<UClass>(L"/Game/Athena/AI/Phoebe/BP_PhoebeManager.BP_PhoebeManager_C", ClassClass);

    auto BotManager = GameMode->ServerBotManager;
    if (PhoebeManagerClass && (!BotManager || !BotManager->IsA(PhoebeManagerClass)))
    {
        if (auto PhoebeManager = UGameplayStatics::SpawnObject(PhoebeManagerClass, GameMode))
        {
            BotManager = PhoebeManager;
            GameMode->ServerBotManager = BotManager;
        }
    }

    if (!BotManager)
    {
        BotLog("[Bots] No ServerBotManager!\n");
        return;
    }

    SetObjectProp(BotManager, "CachedGameMode", GameMode);
    SetObjectProp(BotManager, "CachedGameState", GameMode->GameState);

    if (auto AISystem = GetAISystem())
        if (auto Tracker = GetObjectProp(AISystem, "AIPopulationTracker"))
            SetObjectProp(BotManager, "CachedAIPopulationTracker", Tracker);

    if (!GetObjectProp(BotManager, "CachedBotMutator"))
    {
        auto Mutators = GetActorsOfClass(FindClass("FortAthenaMutator_Bots"));
        AActor* Mutator = Mutators.Num() > 0 ? Mutators[0] : nullptr;
        if (!Mutator)
            if (auto MutatorClass = FindClass("FortAthenaMutator_Bots"))
                Mutator = UWorld::SpawnActor(MutatorClass, FVector{}, FRotator{}, GameMode);

        if (Mutator)
        {
            SetObjectProp(Mutator, "CachedGameMode", GameMode);
            SetObjectProp(Mutator, "CachedGameState", GameMode->GameState);
            SetObjectProp(BotManager, "CachedBotMutator", Mutator);
        }
    }

    auto FoundationsOffset = BotManager->GetOffset("CachedBuildingFoundations");
    if (FoundationsOffset != -1)
    {
        auto& Foundations = *(TArray<AActor*>*)(__int64(BotManager) + FoundationsOffset);
        if (Foundations.Num() == 0)
            for (auto& Foundation : GetActorsOfClass(FindClass("BuildingFoundation")))
                Foundations.Add(Foundation);
    }

    BotLog("[Bots] Bot manager ready (%s).\n", BotManager->Class->Name.ToString().c_str());
}

static bool ContainsWide(const wchar_t* Text, const wchar_t* Needle)
{
    auto NeedleLength = wcslen(Needle);
    for (int i = 0; i < 1024 && Text[i]; i++)
    {
        if (Text[i] == Needle[0] && wcsncmp(Text + i, Needle, NeedleLength) == 0)
            return true;
    }
    return false;
}

static uintptr_t FindStringUse(const wchar_t* Needle)
{
    auto Text = Memcury::PE::Section::GetSection(".text");
    auto RData = Memcury::PE::Section::GetSection(".rdata");
    auto Bytes = (uint8_t*)Text.GetSectionStart().Get();
    auto Size = Text.GetSectionSize();

    for (uint32_t i = 1; i + 6 < Size; i++)
    {
        if (Bytes[i] != 0x8D || (Bytes[i - 1] & 0xFB) != 0x48 || (Bytes[i + 1] & 0xC7) != 0x05)
            continue;

        auto Target = (uintptr_t)&Bytes[i + 6] + *(int32_t*)&Bytes[i + 2];
        if (!RData.isInSection(Memcury::PE::Address(Target)))
            continue;

        auto String = (const wchar_t*)Target;
        if (String[0] >= 0x20 && String[0] < 0x7F && ContainsWide(String, Needle))
            return (uintptr_t)&Bytes[i - 1];

        auto Pointed = *(uintptr_t*)Target;
        if (RData.isInSection(Memcury::PE::Address(Pointed)))
        {
            auto PointedString = (const wchar_t*)Pointed;
            if (PointedString[0] >= 0x20 && PointedString[0] < 0x7F && ContainsWide(PointedString, Needle))
                return (uintptr_t)&Bytes[i - 1];
        }
    }
    return 0;
}

static bool LooksLikePrologue(uintptr_t Ptr)
{
    auto B = (uint8_t*)Ptr;
    return (B[0] == 0x48 && (B[1] == 0x89 || B[1] == 0x8B || B[1] == 0x83 || B[1] == 0x81)) || (B[0] == 0x4C && B[1] == 0x89) ||
           (B[0] == 0x40 && B[1] >= 0x53 && B[1] <= 0x57) || (B[0] == 0x41 && B[1] >= 0x54 && B[1] <= 0x57) || B[0] == 0x55 || B[0] == 0x53 ||
           B[0] == 0x56 || B[0] == 0x57;
}

static uintptr_t FindFunctionStart(const UClass* ComponentClass, uintptr_t Address, bool& bConfirmed)
{
    bConfirmed = false;

    uintptr_t Start = 0;
    for (uintptr_t Ptr = Address & ~(uintptr_t)0xF; Address - Ptr < 0x4000; Ptr -= 0x10)
        if (*(uint8_t*)(Ptr - 1) == 0xCC && LooksLikePrologue(Ptr))
        {
            Start = Ptr;
            break;
        }

    if (!Start)
        return 0;

    if (ComponentClass)
    {
        auto Text = Memcury::PE::Section::GetSection(".text");
        for (int Index = 0; Index < TUObjectArray::Num() && !bConfirmed; Index++)
        {
            auto Object = TUObjectArray::GetObjectByIndex(Index);
            if (!Object || !Object->IsDefaultObject() || !Object->IsA(ComponentClass))
                continue;

            auto Vft = (uintptr_t*)Object->Vft;
            for (int i = 0; i < 400; i++)
            {
                if (!Text.isInSection(Memcury::PE::Address(Vft[i])))
                    break;
                if (Vft[i] == Start)
                    bConfirmed = true;
            }
        }
    }

    return Start;
}

static void (*InventoryOnSpawnedOG)(UObject*, AActor*, void*, void*);

static void EnsureControllerInventory(AActor* Pawn)
{
    static auto PawnClass = FindClass("Pawn");
    if (!Pawn || !Pawn->IsA(PawnClass))
        return;

    auto Controller = (ABotController*)GetObjectProp(Pawn, "Controller");
    if (!Controller || Controller->GetOffset("Inventory") == -1 || Controller->Inventory)
        return;

    if (auto InventoryClass = FindClass("FortInventory"))
        Controller->Inventory = (AFortInventory*)UWorld::SpawnActor(InventoryClass, FVector{}, FRotator{}, Controller);
}

static void GuardedEnsureControllerInventory(AActor* Pawn)
{
    GCrashGuardDepth++;
    __try
    {
        EnsureControllerInventory(Pawn);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    GCrashGuardDepth--;
}

static void InventoryOnSpawned(UObject* Component, AActor* Pawn, void* Arg3, void* Arg4)
{
    GuardedEnsureControllerInventory(Pawn);
    return InventoryOnSpawnedOG(Component, Pawn, Arg3, Arg4);
}

static void HookBotInventorySetup()
{
    static bool bHooked = false;
    if (bHooked)
        return;
    bHooked = true;

    auto Use = FindStringUse(L"Can't create/initialize inventory on AI");
    if (!Use)
    {
        BotLog("[Bots] Couldn't find the AI inventory setup function, bots may not start their AI.\n");
        return;
    }

    auto ComponentClass = FindClass("FortAthenaAISpawnerDataComponent_InventoryBase");
    bool bConfirmed = false;
    auto Function = FindFunctionStart(ComponentClass, Use, bConfirmed);
    auto Base = (uintptr_t)GetModuleHandleW(nullptr);
    if (!Function)
    {
        BotLog("[Bots] Found the AI inventory message (+0x%llx) but not its function, not hooking.\n", (unsigned long long)(Use - Base));
        return;
    }

    if (!bConfirmed)
    {
        BotLog("[Bots] AI inventory setup at +0x%llx isn't a known virtual, not hooking (message at +0x%llx).\n", (unsigned long long)(Function - Base),
               (unsigned long long)(Use - Base));
        return;
    }

    Hooking::Hook(Function, InventoryOnSpawned, InventoryOnSpawnedOG);
    BotLog("[Bots] Hooked AI inventory setup at +0x%llx.\n", (unsigned long long)(Function - Base));
}

class UAthenaAIServicePlayerBots : public UObject
{
public:
    UCLASS_COMMON_MEMBERS(UAthenaAIServicePlayerBots);

    DEFINE_BITFIELD_PROP(bDoSpawnBotFlow);

    DEFINE_FUNC(OnPlaylistDataReady, void);
    DEFINE_FUNC(OnGamePhaseLogicReady, void);
};

static UObject* EnsurePlayerBotService(AFortGameMode* GameMode)
{
    static UObject* Service = nullptr;
    if (Service)
        return Service;

    auto ServiceClass = UAthenaAIServicePlayerBots::StaticClass();
    auto AISystem = GetAISystem();
    auto ServiceManager = GetObjectProp(AISystem, "AIServiceManager");
    if (!ServiceClass || !ServiceManager)
        return nullptr;

    auto ServicesOffset = ServiceManager->GetOffset("AIServices");
    if (ServicesOffset == -1)
        return nullptr;

    auto& Services = *(TArray<UObject*>*)(__int64(ServiceManager) + ServicesOffset);
    for (auto& Existing : Services)
        if (Existing && Existing->IsA(ServiceClass))
            Service = Existing;

    bool bCreated = false;
    if (!Service)
    {
        Service = UGameplayStatics::SpawnObject(ServiceClass, ServiceManager);
        if (!Service)
            return nullptr;

        SetObjectProp(Service, "CachedGameMode", GameMode);
        SetObjectProp(Service, "CachedGameState", GameMode->GameState);
        SetObjectProp(Service, "AIServiceManager", ServiceManager);
        Services.Add(Service);
        bCreated = true;
    }

    if (auto AISettings = GetObjectProp(GameMode, "AISettings"))
    {
        auto ClassesOffset = AISettings->GetOffset("AIServices");
        if (ClassesOffset != -1)
        {
            auto& Classes = *(TArray<const UClass*>*)(__int64(AISettings) + ClassesOffset);
            bool bListed = false;
            for (auto& Class : Classes)
                if (Class == ServiceClass)
                    bListed = true;
            if (!bListed)
                Classes.Add(ServiceClass);
        }
    }

    auto PlayerBots = (UAthenaAIServicePlayerBots*)Service;
    if (PlayerBots->HasbDoSpawnBotFlow())
        PlayerBots->bDoSpawnBotFlow = false;

    if (bCreated)
    {
        auto GameState = GameMode->GameState;
        auto Playlist = GameState->HasCurrentPlaylistInfo() ? GameState->CurrentPlaylistInfo.BasePlaylist : nullptr;
        FGameplayTagContainer PlaylistContextTags{};
        if (Playlist)
            PlayerBots->OnPlaylistDataReady(GameState, Playlist, PlaylistContextTags);
        if (auto GamePhaseLogic = GetGamePhaseLogic())
            PlayerBots->OnGamePhaseLogicReady(FWeakObjectPtr(GamePhaseLogic));
    }

    BotLog("[Bots] Player bot service %s.\n", bCreated ? "created" : "already running");
    return Service;
}

static bool EnsureBotSystems(AFortGameMode* GameMode)
{
    static bool bReady = false;
    static int Attempts = 0;
    if (bReady || Attempts >= 3)
        return bReady;
    Attempts++;

    if (!GameMode->HasServerBotManager())
        BotLog("[Bots] This build has no ServerBotManager on the game mode, skipping bot manager setup.\n");
    else
        SetupBotManager(GameMode);

    auto AISystem = GetAISystem();
    if (!AISystem || !GetObjectProp(AISystem, "AISpawner"))
    {
        BotLog("[Bots] The world has no AI spawner yet (attempt %d).\n", Attempts);
        return false;
    }

    HookBotInventorySetup();

    if (!EnsurePlayerBotService(GameMode))
    {
        BotLog("[Bots] Couldn't register the player bot service, not spawning bots (attempt %d).\n", Attempts);
        return false;
    }

    return bReady = GetPhoebeSpawnerData() != nullptr;
}

static bool RequestBotSpawn(const FVector& Location)
{
    auto AISystem = GetAISystem();
    auto Spawner = (UBotAISpawner*)GetObjectProp(AISystem, "AISpawner");
    auto SpawnerData = GetPhoebeSpawnerData();
    if (!Spawner || !SpawnerData)
        return false;

    auto ComponentList = UFortAthenaAISpawnerData::CreateComponentListFromClass(SpawnerData, UWorld::GetWorld());
    if (!ComponentList)
        return false;

    FTransform Transform{};
    FQuat Rotation{};
    Rotation.W = 1.0;
    FVector Translation = Location;
    FVector Scale(1, 1, 1);
    Transform.Rotation = Rotation;
    Transform.Translation = Translation;
    Transform.Scale3D = Scale;

    Spawner->RequestSpawn(ComponentList, Transform);
    RequestedBots++;
    PendingSpawns++;
    return true;
}

static FVector PickWarmupSpawn()
{
    static TArray<AActor*> Starts;
    if (Starts.Num() == 0)
    {
        Starts = GetActorsOfClass(FindClass("FortPlayerStartWarmup"));
        if (Starts.Num() == 0)
            Starts = GetActorsOfClass(FindClass("PlayerStart"));
    }

    if (Starts.Num() == 0)
        return FVector(0, 0, 10000);

    auto Location = Starts[RandInt(0, Starts.Num() - 1)]->K2_GetActorLocation();
    Location.X += RandRange(-300.f, 300.f);
    Location.Y += RandRange(-300.f, 300.f);
    Location.Z += 100.f;
    return Location;
}

AActor* Bots::SpawnBot(FVector Location)
{
    auto GameMode = GetGameMode();
    if (!GameMode || !EnsureBotSystems(GameMode))
        return nullptr;

    return RequestBotSpawn(Location) ? (AActor*)GetObjectProp(GetAISystem(), "AISpawner") : nullptr;
}

static int32 GetHumanCount(AFortGameMode* GameMode)
{
    int32 Count = 0;
    for (auto& Player : GameMode->AlivePlayers)
        if (Player && Player->IsA<AFortPlayerControllerAthena>())
            Count++;
    return Count;
}

static void SpawnPendingBots(AFortGameMode* GameMode, uint8 GamePhase)
{
    if ((EAthenaGamePhase)GamePhase != EAthenaGamePhase::Warmup)
        return;

    auto Humans = GetHumanCount(GameMode);
    if (FConfiguration::bBotsWaitForHuman && Humans == 0)
        return;

    if (!EnsureBotSystems(GameMode))
        return;

    int32 Wanted = FConfiguration::bBotsFillLobby ? FConfiguration::BotCount - Humans : FConfiguration::BotCount;
    int32 ToSpawn = (std::min)(Wanted - RequestedBots, FConfiguration::BotsPerTick);

    for (int32 i = 0; i < ToSpawn; i++)
        if (!RequestBotSpawn(PickWarmupSpawn()))
            break;
}

static std::unordered_set<std::string> UsedBotNames;

static std::string LowerName(const std::string& Name)
{
    std::string Lower = Name;
    for (auto& Ch : Lower)
        Ch = (char)tolower((unsigned char)Ch);
    return Lower;
}

static bool BotNameTaken(const std::string& Name)
{
    auto Lower = LowerName(Name);
    if (UsedBotNames.count(Lower))
        return true;

    auto GameMode = GetGameMode();
    auto GameState = GameMode ? (AFortGameStateAthena*)GameMode->GameState : nullptr;
    if (!GameState || !GameState->HasPlayerArray())
        return false;

    for (int i = 0; i < GameState->PlayerArray.Num(); i++)
    {
        auto PlayerState = (AFortPlayerStateAthena*)GameState->PlayerArray[i];
        if (!PlayerState || !PlayerState->HasPlayerNamePrivate())
            continue;
        auto Wide = PlayerState->PlayerNamePrivate.ToString();
        std::string Existing;
        for (auto Ch : Wide)
            Existing += (char)Ch;
        if (LowerName(Existing) == Lower)
            return true;
    }
    return false;
}

static FString MakeBotName()
{
    static const char* Prefixes[] = { "Shadow", "Blaze", "Frost", "Viper", "Nova", "Rogue", "Echo", "Pixel", "Turbo", "Ghost", "Storm", "Lunar", "Hyper", "Crimson", "Atomic", "Silent" };
    static const char* Suffixes[] = { "Wolf", "Sniper", "Fox", "Ninja", "Rider", "Hawk", "Byte", "King", "Llama", "Raven", "Bolt", "Drift", "Fury", "Spark", "Ace", "Tiger" };

    std::string Name;
    for (int Try = 0; Try < 64; Try++)
    {
        Name = std::string(Prefixes[RandInt(0, 15)]) + Suffixes[RandInt(0, 15)];
        if (RandInt(0, 2) != 0)
            Name += std::to_string(RandInt(1, 9999));
        if (!BotNameTaken(Name))
            break;
        Name.clear();
    }

    if (Name.empty())
    {
        std::string Base = std::string(Prefixes[RandInt(0, 15)]) + Suffixes[RandInt(0, 15)];
        for (int Number = 1; Name.empty() || BotNameTaken(Name); Number++)
            Name = Base + std::to_string(Number);
    }

    UsedBotNames.insert(LowerName(Name));
    return FString(std::wstring(Name.begin(), Name.end()).c_str());
}

static void LogBotNameCheck()
{
    auto GameMode = GetGameMode();
    auto GameState = GameMode ? (AFortGameStateAthena*)GameMode->GameState : nullptr;
    if (!GameState || !GameState->HasPlayerArray())
        return;

    std::unordered_map<std::string, int> Counts;
    int Players = 0, Duplicates = 0, Starred = 0;
    for (int i = 0; i < GameState->PlayerArray.Num(); i++)
    {
        auto PlayerState = (AFortPlayerStateAthena*)GameState->PlayerArray[i];
        if (!PlayerState || !PlayerState->HasPlayerNamePrivate())
            continue;
        auto Wide = PlayerState->PlayerNamePrivate.ToString();
        std::string Name;
        for (auto Ch : Wide)
            Name += (char)Ch;
        Players++;
        if (Name.find('*') != std::string::npos)
            Starred++;
        if (++Counts[LowerName(Name)] == 2)
            Duplicates++;
    }

    BotLog("[NAMES] %d players on the server: %d name(s) used more than once, %d name(s) with a '*' in them (both should be 0).\n", Players, Duplicates, Starred);
}

static void AddGameMemberInfo(AFortGameStateAthena* GameState, AFortPlayerStateAthena* PlayerState)
{
    if (!GameState->HasGameMemberInfoArray())
        return;

    auto Member = (FGameMemberInfo*)malloc(FGameMemberInfo::Size());
    memset((PBYTE)Member, 0, FGameMemberInfo::Size());

    Member->MostRecentArrayReplicationKey = -1;
    Member->ReplicationID = -1;
    Member->ReplicationKey = -1;
    Member->TeamIndex = PlayerState->TeamIndex;
    Member->SquadId = PlayerState->SquadId;
    Member->MemberUniqueId = PlayerState->HasUniqueID() ? PlayerState->UniqueID : PlayerState->UniqueId;

    auto& NewMember = GameState->GameMemberInfoArray.Members.Add(*Member, FGameMemberInfo::Size());
    GameState->GameMemberInfoArray.MarkItemDirty(NewMember);

    auto NotifyGameMemberAdded = (void (*)(AFortGameStateAthena*, uint8_t, uint8_t, FUniqueNetIdRepl*))NotifyGameMemberAdded_;
    if (NotifyGameMemberAdded)
        NotifyGameMemberAdded(GameState, Member->SquadId, Member->TeamIndex, &Member->MemberUniqueId);

    free(Member);
}

static void FixTeam(AFortGameMode* GameMode, AFortPlayerStateAthena* PlayerState, AActor* Controller)
{
    bool bShared = PlayerState->TeamIndex < 3;
    for (auto& Player : GameMode->AlivePlayers)
    {
        auto PlayerController = (AFortPlayerControllerAthena*)Player;
        if (PlayerController && PlayerController->PlayerState && PlayerController->PlayerState->TeamIndex == PlayerState->TeamIndex)
            bShared = true;
    }
    for (auto& Bot : AllBots)
    {
        auto Other = GetWeak<AFortPlayerStateAthena>(Bot.PlayerState);
        if (Other && Other != PlayerState && Other->TeamIndex == PlayerState->TeamIndex)
            bShared = true;
    }

    if (!bShared)
        return;

    auto OldTeam = PlayerState->TeamIndex;
    PlayerState->TeamIndex = AFortGameMode::PickTeam(GameMode, 0, (AFortPlayerControllerAthena*)Controller);
    ((AFortPlayerStateAthenaBot*)PlayerState)->OnRep_TeamIndex(OldTeam);

    if (PlayerState->HasSquadId())
    {
        PlayerState->SquadId = PlayerState->TeamIndex - 3;
        PlayerState->OnRep_SquadId();
    }

    AddGameMemberInfo(GameMode->GameState, PlayerState);
}

static bool IsTracked(const AActor* Controller);

static bool IsRangedWeapon(const UFortItemDefinition* Definition)
{
    static auto RangedClass = FindClass("FortWeaponRangedItemDefinition");
    return Definition && Definition->IsA(RangedClass);
}

static bool IsGun(const UFortItemDefinition* Definition)
{
    static std::unordered_map<const UFortItemDefinition*, bool> Known;
    if (!Definition)
        return false;
    auto Found = Known.find(Definition);
    if (Found != Known.end())
        return Found->second;

    bool bGun = false;
    if (IsRangedWeapon(Definition))
        if (auto Stats = AFortInventory::GetStats((const UFortWeaponItemDefinition*)Definition))
            bGun = FFortRangedWeaponStats::HasDmgPB() && Stats->DmgPB > 0.f;
    Known[Definition] = bGun;
    return bGun;
}

static constexpr int32 BotSlotCount = 5;

static const char* const BrokenBotWeapons[] = {
    "WID_Launcher_HomingRocket_Athena_SR",
    "WID_Athena_Sentry_Turret_Deployable",
    "WID_Paprika_ClusterBomb",
    "WID_Athena_Grenade_EMP",
    "WID_Akita_Galileo_Cosmos_AR_UC",
    "WID_ChainWhip",
    "WID_Crossbow_FNE_Jethro_GuineaPig_Athena_UR",
    "WID_Athena_SunRose_Zeus_Lightning",
};

static std::string WithoutRarity(const std::string& Name)
{
    static const char* const Rarities[] = { "_UC", "_VR", "_SR", "_UR", "_C", "_R" };
    for (auto Rarity : Rarities)
    {
        size_t Length = strlen(Rarity);
        if (Name.size() > Length && Name.compare(Name.size() - Length, Length, Rarity) == 0)
            return Name.substr(0, Name.size() - Length);
    }
    return Name;
}

static bool IsBrokenBotWeapon(const UFortItemDefinition* Definition)
{
    static std::unordered_map<const UFortItemDefinition*, bool> Known;
    if (!Definition)
        return false;
    auto Found = Known.find(Definition);
    if (Found != Known.end())
        return Found->second;

    auto OnList = [](const UFortItemDefinition* Candidate) {
        if (!Candidate)
            return false;
        auto Name = WithoutRarity(((UFortItemDefinition*)Candidate)->Name.ToString().c_str());
        for (auto Listed : BrokenBotWeapons)
            if (Name == WithoutRarity(Listed))
                return true;
        return false;
    };

    bool bBroken = OnList(Definition);
    if (!bBroken)
        if (auto Gadget = Definition->Cast<UFortGadgetItemDefinition>())
            bBroken = OnList(Gadget->GetWeaponItemDefinition());

    Known[Definition] = bBroken;
    if (bBroken)
        BotLog("[Bots] %s is on the broken-weapon list, bots never take it.\n", ((UFortItemDefinition*)Definition)->Name.ToString().c_str());
    return bBroken;
}

static bool IsAmmo(const UFortItemDefinition* Definition)
{
    return Definition && Definition->ItemType == EFortItemType::GetAmmo();
}

static int32 BotAmmoCap(const UFortItemDefinition* Ammo)
{
    static std::unordered_map<const UFortItemDefinition*, int32> Known;
    auto Found = Known.find(Ammo);
    if (Found != Known.end())
        return Found->second;

    auto Name = LowerName(((UFortItemDefinition*)Ammo)->Name.ToString().c_str());
    int32 Cap = 60;
    if (Name.find("shell") != std::string::npos)
        Cap = 30;
    else if (Name.find("heavy") != std::string::npos)
        Cap = 24;
    else if (Name.find("rocket") != std::string::npos || Name.find("explosive") != std::string::npos || Name.find("missile") != std::string::npos)
        Cap = 8;
    else if (Name.find("light") != std::string::npos || Name.find("medium") != std::string::npos)
        Cap = 120;

    Known[Ammo] = Cap;
    return Cap;
}

static int32 BotAmmoWithGun(const UFortItemDefinition* Ammo)
{
    return (std::max)(BotAmmoCap(Ammo) / 2, 1);
}

static FFortRangedWeaponStats* GetGunStats(const UFortItemDefinition* Definition);

static FFortItemEntry* AddBotItem(ABotController* Controller, const UFortItemDefinition* Definition, int32 Count, int32 LoadedAmmo, int32 PhantomReserveAmmo = -1)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory || !Definition || Count <= 0)
        return nullptr;

    int32 Cap = IsAmmo(Definition) ? BotAmmoCap(Definition) : 0;

    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (Entry.ItemDefinition != Definition || IsGun(Definition))
            continue;

        int32 NewCount = Entry.Count + Count;
        if (Cap > 0)
            NewCount = (std::min)(NewCount, (std::max)(Cap, Entry.Count));
        if (NewCount == Entry.Count)
            return &Entry;

        Entry.Count = NewCount;
        Inventory->Inventory.MarkItemDirty(Entry);
        for (auto& Instance : Inventory->Inventory.ItemInstances)
            if (Instance && Instance->ItemEntry.ItemGuid == Entry.ItemGuid)
                Instance->ItemEntry.Count = Entry.Count;
        Inventory->HandleInventoryLocalUpdate();
        return &Entry;
    }

    if (Cap > 0)
        Count = (std::min)(Count, Cap);

    auto Item = (UFortWorldItem*)Definition->CreateTemporaryItemInstanceBP(Count, 0);
    if (!Item)
        return nullptr;

    Item->SetOwningControllerForTemporaryItem(Controller);
    if (Item->HasOwnerInventory())
        Item->OwnerInventory = Inventory;
    else if (Item->HasOwnerInventoryWeak())
        Item->OwnerInventoryWeak = Inventory;
    Item->ItemEntry.ParentInventory = Inventory;
    Item->ItemEntry.LoadedAmmo = LoadedAmmo;

    if (FFortItemEntry::HasPhantomReserveAmmo())
    {
        auto Weapon = Definition->Cast<UFortWeaponItemDefinition>();
        if (Weapon && Weapon->HasbUsesPhantomReserveAmmo() && Weapon->bUsesPhantomReserveAmmo)
        {
            int32 Reserve = PhantomReserveAmmo;
            if (Reserve <= 0)
                if (auto Stats = GetGunStats(Weapon))
                    Reserve = (std::max)(Stats->ClipSize * 2, 0);
            if (Reserve < 0)
                Reserve = 0;
            Item->ItemEntry.PhantomReserveAmmo = Reserve;
        }
    }

    CoCreateGuid((GUID*)&Item->ItemEntry.ItemGuid);
    if (FFortItemEntry::HasTrackerGuid())
        CoCreateGuid((GUID*)&Item->ItemEntry.TrackerGuid);

    auto& NewEntry = Entries.Add(Item->ItemEntry, FFortItemEntry::Size());
    NewEntry.bIsReplicatedCopy = true;
    Inventory->Inventory.ItemInstances.Add(Item);
    Inventory->Inventory.MarkItemDirty(NewEntry);
    Inventory->HandleInventoryLocalUpdate();
    return &NewEntry;
}

static bool HasRangedWeapon(ABotController* Controller)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory)
        return false;

    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
        if (IsGun(Entries.Get(i, FFortItemEntry::Size()).ItemDefinition))
            return true;
    return false;
}

static void EquipEntry(ABotPawn* Pawn, FFortItemEntry* Entry)
{
    auto Definition = Entry->ItemDefinition;
    if (auto Gadget = Definition->Cast<UFortGadgetItemDefinition>())
        Definition = Gadget->GetWeaponItemDefinition();
    Pawn->EquipWeaponDefinition(Definition, Entry->ItemGuid, Entry->TrackerGuid, false);
}

static const char* GunKind(const UFortItemDefinition* Definition);

static bool TakesSlot(const UFortItemDefinition* Definition)
{
    return Definition && AFortInventory::IsPrimaryQuickbar(Definition);
}

static int32 ItemScore(const UFortItemDefinition* Definition)
{
    int32 Rarity = Definition && Definition->HasRarity() ? (int32)Definition->Rarity : 0;
    return (IsGun(Definition) ? 100 : 0) + Rarity;
}

static int32 CountBotItem(ABotController* Controller, const UFortItemDefinition* Definition)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory)
        return 0;

    int32 Count = 0;
    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (Entry.ItemDefinition == Definition && Entry.Count > 0)
            Count += Entry.Count;
    }
    return Count;
}

static int32 CountBotSlots(ABotController* Controller)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory)
        return 0;

    int32 Used = 0;
    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (Entry.Count > 0 && TakesSlot(Entry.ItemDefinition))
            Used++;
    }
    return Used;
}

static FFortItemEntry* PickSlotToFree(ABotController* Controller, const UFortItemDefinition* Definition, bool& bOutRoom)
{
    bOutRoom = true;
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory || !TakesSlot(Definition))
        return nullptr;

    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (Entry.Count > 0 && Entry.ItemDefinition == Definition && !IsGun(Definition))
            return nullptr;
    }

    if (CountBotSlots(Controller) < BotSlotCount)
        return nullptr;

    int32 NewScore = ItemScore(Definition);
    std::string Kind = IsGun(Definition) ? GunKind(Definition) : "";
    FFortItemEntry* SameKind = nullptr;
    FFortItemEntry* Lowest = nullptr;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (Entry.Count <= 0 || !TakesSlot(Entry.ItemDefinition))
            continue;

        int32 Score = ItemScore(Entry.ItemDefinition);
        if (!Lowest || Score < ItemScore(Lowest->ItemDefinition))
            Lowest = &Entry;
        if (!Kind.empty() && IsGun(Entry.ItemDefinition) && Kind == GunKind(Entry.ItemDefinition) && (!SameKind || Score < ItemScore(SameKind->ItemDefinition)))
            SameKind = &Entry;
    }

    FFortItemEntry* Out = SameKind ? SameKind : Lowest;
    if (Out && ItemScore(Out->ItemDefinition) < NewScore)
        return Out;

    bOutRoom = false;
    return nullptr;
}

static bool BotWantsItem(ABotController* Controller, const UFortItemDefinition* Definition)
{
    if (!Definition || IsBrokenBotWeapon(Definition))
        return false;
    if (IsAmmo(Definition))
        return CountBotItem(Controller, Definition) < BotAmmoCap(Definition);

    bool bRoom = true;
    PickSlotToFree(Controller, Definition, bRoom);
    return bRoom;
}

static void DropBotEntry(ABotController* Controller, ABotPawn* Pawn, FFortItemEntry* Entry)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory || !Pawn || !Entry)
        return;

    FGuid Guid = Entry->ItemGuid;
    AFortInventory::SpawnPickup(Pawn->K2_GetActorLocation(), *Entry, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(), Pawn);
    Inventory->Remove(Guid);
}

static int32 GPickupsDeclined = 0;
static int32 GPickupsSwapped = 0;

void Bots::OnPickup(AActor* ControllerActor, AActor* PawnActor, FFortItemEntry* PickupEntry)
{
    if (!PickupEntry || !PickupEntry->ItemDefinition || !IsTracked(ControllerActor))
        return;

    auto Controller = (ABotController*)ControllerActor;
    auto Pawn = (ABotPawn*)PawnActor;
    auto Definition = PickupEntry->ItemDefinition;
    bool bHadRanged = HasRangedWeapon(Controller);

    if (!BotWantsItem(Controller, Definition))
    {
        if (Pawn)
            AFortInventory::SpawnPickup(Pawn->K2_GetActorLocation(), *PickupEntry, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetUnset(), Pawn);
        if (++GPickupsDeclined <= 5)
            BotLog("[Bots] A bot left %s on the ground (%s).\n", ((UFortItemDefinition*)Definition)->Name.ToString().c_str(),
                   IsBrokenBotWeapon(Definition) ? "broken-weapon list" : IsAmmo(Definition) ? "already carries enough of that ammo" : "all 5 slots full of better items");
        return;
    }

    bool bRoom = true;
    bool bSwapped = false;
    if (auto Old = PickSlotToFree(Controller, Definition, bRoom))
    {
        bool bWasHeld = Pawn && Pawn->CurrentWeapon && ((AFortWeapon*)Pawn->CurrentWeapon)->ItemEntryGuid == Old->ItemGuid;
        if (++GPickupsSwapped <= 5)
            BotLog("[Bots] A bot with 5 full slots swapped %s for %s (the old one is on the ground).\n", ((UFortItemDefinition*)Old->ItemDefinition)->Name.ToString().c_str(),
                   ((UFortItemDefinition*)Definition)->Name.ToString().c_str());
        DropBotEntry(Controller, Pawn, Old);
        bSwapped = bWasHeld;
    }

    auto Entry = AddBotItem(Controller, Definition, PickupEntry->Count, PickupEntry->LoadedAmmo, FFortItemEntry::HasPhantomReserveAmmo() ? PickupEntry->PhantomReserveAmmo : -1);
    if (!Entry || !IsGun(Definition))
        return;

    if ((!bHadRanged || bSwapped) && Pawn)
        EquipEntry(Pawn, Entry);

    if (auto Ammo = ((UFortWorldItemDefinition*)Definition)->GetAmmoWorldItemDefinition_BP())
        if (Ammo != Definition)
            AddBotItem(Controller, Ammo, BotAmmoWithGun(Ammo), 0);
}

bool Bots::WantsPickup(AActor* ControllerActor, AActor* PickupActor)
{
    if (!ControllerActor || !PickupActor || !IsTracked(ControllerActor) || !PickupActor->IsA<AFortPickupAthena>())
        return true;
    return BotWantsItem((ABotController*)ControllerActor, ((AFortPickupAthena*)PickupActor)->PrimaryPickupItemEntry.ItemDefinition);
}

static FName ResolveLootTierGroup(const wchar_t* Name)
{
    FName Group(Name);
    auto GameMode = GetGameMode();
    if (GameMode && VersionInfo.FortniteVersion >= 20 && GameMode->HasRedirectAthenaLootTierGroups())
    {
        static auto RedirectOffset = GameMode->GetOffset("RedirectAthenaLootTierGroups");
        auto& Redirects = *(TMap<int32, int32>*)(__int64(GameMode) + RedirectOffset);
        for (const auto& [OldGroup, NewGroup] : Redirects)
        {
            if (OldGroup == Group.ComparisonIndex)
            {
                Group.ComparisonIndex = NewGroup;
                break;
            }
        }
    }
    return Group;
}

static const char* GunKind(const UFortItemDefinition* Definition)
{
    auto Name = ((UFortItemDefinition*)Definition)->Name.ToString();
    if (Name.find("Shotgun") != std::string::npos)
        return "shotgun";
    if (Name.find("SMG") != std::string::npos || Name.find("Pistol") != std::string::npos)
        return "smg";
    if (Name.find("Sniper") != std::string::npos || Name.find("DMR") != std::string::npos)
        return "sniper";
    if (Name.find("Assault") != std::string::npos || Name.find("Rifle") != std::string::npos)
        return "rifle";
    return "other";
}

static void GiveSeasonLoadout(ABotController* Controller, ABotPawn* Pawn)
{
    if (!Controller || !Pawn || !Controller->Inventory)
        return;

    static const wchar_t* TierGroups[] = { L"Loot_AthenaTreasure", L"Loot_Treasure", L"Loot_AthenaFloorLoot" };

    std::vector<const UFortItemDefinition*> Guns;
    std::vector<std::string> Kinds;

    for (auto GroupName : TierGroups)
    {
        auto Group = ResolveLootTierGroup(GroupName);

        for (int Roll = 0; Roll < 10 && Guns.size() < 3; Roll++)
        {
            TArray<FFortItemEntry*> Drops{};
            UFortLootPackage::ChooseLootForContainer(Drops, Group);

            for (auto& Drop : Drops)
            {
                if (!Drop)
                    continue;

                auto Definition = (const UFortItemDefinition*)Drop->ItemDefinition;
                if (Guns.size() < 3 && Definition && IsGun(Definition) && !IsBrokenBotWeapon(Definition))
                {
                    std::string Kind = GunKind(Definition);
                    bool bTaken = false;
                    for (size_t i = 0; i < Guns.size(); i++)
                        if (Guns[i] == Definition || Kinds[i] == Kind)
                            bTaken = true;

                    if (!bTaken)
                    {
                        Guns.push_back(Definition);
                        Kinds.push_back(Kind);
                    }
                }

                free(Drop);
            }

            Drops.Free();
        }

        if (Guns.size() >= 3)
            break;
    }

    FGuid FirstGuid{};
    bool bHaveFirst = false;
    std::string Summary;

    for (auto Definition : Guns)
    {
        auto Stats = AFortInventory::GetStats((const UFortWeaponItemDefinition*)Definition);
        int Clip = Stats ? Stats->ClipSize : 0;
        if (Clip <= 0)
            continue;

        bool bRoom = true;
        if (PickSlotToFree(Controller, Definition, bRoom) || !bRoom)
            continue;

        auto Entry = AddBotItem(Controller, Definition, 1, Clip);
        if (!Entry)
            continue;

        if (!bHaveFirst)
        {
            FirstGuid = Entry->ItemGuid;
            bHaveFirst = true;
        }

        if (auto Ammo = ((UFortWorldItemDefinition*)Definition)->GetAmmoWorldItemDefinition_BP())
            if ((const void*)Ammo != (const void*)Definition)
                AddBotItem(Controller, Ammo, BotAmmoWithGun(Ammo), 0);

        if (!Summary.empty())
            Summary += ", ";
        Summary += ((UFortItemDefinition*)Definition)->Name.ToString().c_str();
    }

    if (bHaveFirst)
    {
        auto& Entries = Controller->Inventory->Inventory.ReplicatedEntries;
        for (int i = 0; i < Entries.Num(); i++)
        {
            auto& Entry = Entries.Get(i, FFortItemEntry::Size());
            if (Entry.ItemGuid == FirstGuid)
            {
                EquipEntry(Pawn, &Entry);
                break;
            }
        }
    }

    static int Reported = 0;
    static bool bWarnedEmpty = false;
    if (Summary.empty())
    {
        if (!bWarnedEmpty)
        {
            bWarnedEmpty = true;
            BotLog("[Bots] Season loadout: %s, bots keep the pickaxe and loot the floor instead.\n",
                   Guns.empty() ? "the loot tables gave no guns" : "no rolled gun had loaded stats");
        }
    }
    else if (++Reported <= 10)
        BotLog("[Bots] Season loadout: %s\n", Summary.c_str());
}

static void InitBotAbilities(AFortPlayerStateAthena* PlayerState)
{
    auto Interface = PlayerState->GetInterface(IFortAbilitySystemInterface::StaticClass());
    if (InitializePlayerGameplayAbilities_ && Interface)
        ((void (*&)(const IInterface*))InitializePlayerGameplayAbilities_)(Interface);
    else if (PlayerState->AbilitySystemComponent)
        for (auto& AbilitySet : AFortGameMode::AbilitySets)
            PlayerState->AbilitySystemComponent->GiveAbilitySet(AbilitySet);
}

static void GiveStartingItems(AFortGameMode* GameMode, ABotController* Controller, ABotPawn* Pawn)
{
    static auto Pickaxe = FindObject<UFortItemDefinition>(L"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01");
    static auto SmartItemDefClass = FindClass("FortSmartBuildingItemDefinition");

    for (int i = 0; i < GameMode->StartingItems.Num(); i++)
    {
        auto& StartingItem = GameMode->StartingItems.Get(i, FItemAndCount::Size());
        if (StartingItem.Count && StartingItem.Item && (!SmartItemDefClass || !StartingItem.Item->IsA(SmartItemDefClass)) && !StartingItem.Item->IsA<UFortWeaponMeleeItemDefinition>())
            AddBotItem(Controller, StartingItem.Item, StartingItem.Count, 0);
    }

    if (auto Entry = AddBotItem(Controller, Pickaxe, 1, 0))
        EquipEntry(Pawn, Entry);
}

struct FBotSetupContext
{
    AFortGameMode* GameMode;
    ABotController* Controller;
    ABotPawn* Pawn;
    AFortPlayerStateAthena* PlayerState;
};

static bool GuardedStep(const char* Name, void (*Step)(FBotSetupContext&), FBotSetupContext& Context, bool bLog)
{
    if (bLog)
        BotLog("[Bots]   setup: %s\n", Name);

    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        Step(Context);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
        BotLog("[Bots] Setup step '%s' crashed for a bot, skipped it.\n", Name);
    return bOk;
}

static void SetupNewBot(AFortGameMode* GameMode, ABotController* Controller, ABotPawn* Pawn, AFortPlayerStateAthena* PlayerState)
{
    FBot Bot;
    Bot.Controller = FWeakObjectPtr(Controller);
    Bot.Pawn = FWeakObjectPtr(Pawn);
    Bot.PlayerState = FWeakObjectPtr(PlayerState);
    Bot.NextDirectorTime = Now() + RandRange(2.f, 6.f);
    AllBots.push_back(Bot);
    SpawnedBots++;
    PendingSpawns = (std::max)(PendingSpawns - 1, 0);

    bool bLog = SpawnedBots <= 3;
    if (bLog)
        BotLog("[Bots] Setting up bot %d (%s)\n", SpawnedBots, Controller->Class->Name.ToString().c_str());

    FBotSetupContext Context{ GameMode, Controller, Pawn, PlayerState };

    GuardedStep("bot flag", [](FBotSetupContext& C) {
        if (C.PlayerState->HasbIsABot())
            C.PlayerState->bIsABot = true;
    }, Context, bLog);

    GuardedStep("team", [](FBotSetupContext& C) { FixTeam(C.GameMode, C.PlayerState, C.Controller); }, Context, bLog);
    GuardedStep("abilities", [](FBotSetupContext& C) {
        InitBotAbilities(C.PlayerState);
        if (C.Pawn->GetHealth() <= 0.f)
        {
            C.Pawn->SetMaxHealth(100.f);
            C.Pawn->SetHealth(100.f);
        }
    }, Context, bLog);

    GuardedStep("inventory", [](FBotSetupContext& C) {
        if (!C.Controller->Inventory)
            if (auto InventoryClass = FindClass("FortInventory"))
                C.Controller->Inventory = (AFortInventory*)UWorld::SpawnActor(InventoryClass, FVector{}, FRotator{}, C.Controller);
        GiveStartingItems(C.GameMode, C.Controller, C.Pawn);
    }, Context, bLog);

    GuardedStep("name", [](FBotSetupContext& C) {
        C.GameMode->ChangeName(C.Controller, MakeBotName(), true);
        C.PlayerState->OnRep_PlayerName();
    }, Context, bLog);

    GuardedStep("behavior tree", [](FBotSetupContext& C) {
        UpdatePhaseKeys(C.Controller);
        SetBlackboardBool(C.Controller, L"AIEvaluator_Global_IsMovementBlocked", false);
        SetBlackboardBool(C.Controller, L"AIEvaluator_Global_HasEverJumpedFromBusKey", false);
        SetBlackboardBool(C.Controller, L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey", false);
        StartBehaviorTree(C.Controller);
    }, Context, bLog);

    if (SpawnedBots == 1)
    {
        auto Brain = (UBotBrain*)Controller->BrainComponent;
        BotLog("[Bots] First bot spawned: controller %s, pawn %s, brain %s (running %d), blackboard %d, tree %s, inventory %d, ability init %d\n",
               Controller->Class->Name.ToString().c_str(), Pawn->Class->Name.ToString().c_str(), Brain ? Brain->Class->Name.ToString().c_str() : "none",
               Brain ? (int)Brain->IsRunning() : 0, Controller->Blackboard != nullptr, Controller->BehaviorTree ? Controller->BehaviorTree->Name.ToString().c_str() : "none",
               Controller->Inventory != nullptr, InitializePlayerGameplayAbilities_ != 0);
    }
}

static bool IsTracked(const AActor* Controller)
{
    for (auto& Bot : AllBots)
        if (Bot.Controller.Get() == Controller)
            return true;
    return false;
}

static void PickUpNewBots(AFortGameMode* GameMode, double Time)
{
    if (PendingSpawns <= 0 || Time < NextScanTime)
        return;
    NextScanTime = Time + 0.25;

    static auto BotControllerClass = FindClass("FortAthenaAIBotController");
    auto Controllers = GetActorsOfClass(BotControllerClass);
    for (auto& Actor : Controllers)
    {
        if (!Actor || IsTracked(Actor) || Actor->Class->Name.ToString().find("Phoebe") == std::string::npos)
            continue;

        auto Controller = (ABotController*)Actor;
        auto Pawn = (ABotPawn*)Controller->Pawn;
        auto PlayerState = Controller->PlayerState;
        if (!Pawn || !PlayerState)
            continue;

        SetupNewBot(GameMode, Controller, Pawn, PlayerState);
    }
    Controllers.Free();
}

bool Bots::IsBotController(const UObject* Controller)
{
    return Controller && IsTracked((const AActor*)Controller);
}

int32 Bots::GetAliveBotCount()
{
    int32 Count = 0;
    for (auto& Bot : AllBots)
        if (Bot.State != EBotState::Dead)
            Count++;
    return Count;
}

int32 Bots::GetSpawnedBotCount()
{
    return SpawnedBots;
}

static void SetIntPropAndRep(UObject* Object, const char* Name, const char* OnRep, int32 Value)
{
    auto Offset = Object->GetOffset(Name);
    if (Offset == -1)
        return;

    auto& Current = *(int32*)(__int64(Object) + Offset);
    if (Current == Value)
        return;

    Current = Value;
    if (auto Function = Object->GetFunction(OnRep))
        Object->ProcessEvent(Function, nullptr);
}

static void SyncPlayerCounts(AFortGameMode* GameMode, uint8 GamePhase)
{
    auto& AlivePlayers = GameMode->AlivePlayers;
    for (int i = AlivePlayers.Num() - 1; i >= 0; i--)
        if (!AlivePlayers[i] || !AlivePlayers[i]->IsA<AFortPlayerControllerAthena>())
            AlivePlayers.Remove(i);

    auto AliveBotsOffset = GameMode->GetOffset("AliveBots");
    if (AliveBotsOffset != -1)
    {
        auto& AliveBots = *(TArray<AActor*>*)(__int64(GameMode) + AliveBotsOffset);
        for (int i = AliveBots.Num() - 1; i >= 0; i--)
        {
            bool bAlive = false;
            for (auto& Bot : AllBots)
                if (Bot.State != EBotState::Dead && Bot.Controller.Get() == AliveBots[i])
                    bAlive = true;
            if (!bAlive)
                AliveBots.Remove(i);
        }
        for (auto& Bot : AllBots)
        {
            auto Controller = GetWeak(Bot.Controller);
            if (Bot.State == EBotState::Dead || !Controller)
                continue;
            bool bListed = false;
            for (auto& Listed : AliveBots)
                if (Listed == Controller)
                    bListed = true;
            if (!bListed)
                AliveBots.Add(Controller);
        }
    }

    if (!FConfiguration::bBotsSyncPlayersLeft || (EAthenaGamePhase)GamePhase < EAthenaGamePhase::Warmup)
        return;

    auto GameState = GameMode->GameState;
    auto BotCount = Bots::GetAliveBotCount();
    SetIntPropAndRep(GameState, "PlayerBotsLeft", "OnRep_PlayerBotsLeft", BotCount);
    SetIntPropAndRep(GameState, "PlayersLeft", "OnRep_PlayersLeft", AlivePlayers.Num() + BotCount);

    static double NextLogTime = 0;
    if (Now() >= NextLogTime)
    {
        NextLogTime = Now() + 15.0;
        BotLog("[Bots] PlayersLeft on server = %d (%d humans + %d bots, %d spawn requests)\n", GameState->PlayersLeft, AlivePlayers.Num(), BotCount, RequestedBots);
    }
}

static FVector BusStartLocation;
static double BusStartTime = -1;
static FVector BusVelocity;
static bool bBusVelocityKnown = false;
static std::vector<FVector> DropSpots;
static std::vector<int32> DropSpotUsage;
static std::vector<FVector> JumpTargets;
static bool bLoggedJumpSpread = false;

static void PutBotsOnBus()
{
    for (auto& Bot : AllBots)
    {
        auto Controller = GetWeak<ABotController>(Bot.Controller);
        auto Pawn = GetWeak<ABotPawn>(Bot.Pawn);
        if (Bot.State == EBotState::Dead || !Controller || !Pawn)
            continue;

        SetBrainActive(Controller, false);
        SetBlackboardBool(Controller, L"AIEvaluator_Global_IsInBus", true);
        Pawn->SetActorHiddenInGame(true);
        Pawn->SetActorEnableCollision(false);
        if (auto Movement = (UBotMovement*)Pawn->CharacterMovement)
            Movement->DisableMovement();

        Pawn->SetHealth(100.f);
        Bot.State = EBotState::OnBus;
        Bot.JumpTime = -1;
        Bot.ThankTime = -1;
    }

    BusStartTime = -1;
    bBusVelocityKnown = false;

    DropSpots.clear();
    JumpTargets.clear();
    bLoggedJumpSpread = false;
    for (auto& Foundation : GetActorsOfClass(FindClass("BuildingFoundation")))
    {
        if (!Foundation)
            continue;
        auto Location = Foundation->K2_GetActorLocation();
        if (fabs(Location.X) < 1000.0 && fabs(Location.Y) < 1000.0)
            continue;

        bool bDuplicate = false;
        for (auto& Spot : DropSpots)
            if (Dist2D(Spot, Location) < 6000.0)
                bDuplicate = true;
        if (!bDuplicate)
            DropSpots.push_back(Location);
    }
    DropSpotUsage.assign(DropSpots.size(), 0);

    BotLog("[Bots] %d bots boarded the bus (%d drop spots).\n", Bots::GetAliveBotCount(), (int)DropSpots.size());
    LogBotNameCheck();
}

static void PlanJump(FBot& Bot, AFortAthenaAircraft* Aircraft, double Time)
{
    double DropStart = Aircraft->DropStartTime, DropEnd = Aircraft->DropEndTime;
    if (DropEnd <= DropStart)
        DropEnd = DropStart + 30.0;

    double Speed = BusVelocity.Magnitude();
    auto Direction = BusVelocity / Speed;
    FVector Right(-Direction.Y, Direction.X, 0);

    double WindowStart = (std::max)(DropStart + 0.5, Time + 0.5);
    double WindowEnd = DropEnd - 1.0;
    if (WindowEnd <= WindowStart)
        WindowEnd = WindowStart + 1.0;

    std::vector<size_t> Candidates;
    for (size_t i = 0; i < DropSpots.size(); i++)
    {
        auto& Location = DropSpots[i];
        auto Offset = Location - BusStartLocation;
        double Along = Offset.X * Direction.X + Offset.Y * Direction.Y;
        double PassTime = BusStartTime + Along / Speed;
        auto Closest = BusStartLocation + Direction * Along;

        if (PassTime > DropStart + 1.0 && PassTime < DropEnd - 2.0 && Dist2D(Closest, Location) < FConfiguration::BotMaxGlideDistance)
            Candidates.push_back(i);
    }

    bool bUsePoi = Candidates.size() >= 3 && RandRange(0.f, 1.f) < 0.5f;
    double MaxLateral = FConfiguration::BotMaxGlideDistance;
    const double MinSeparation = 8000.0;

    FVector BestTarget = BusStartLocation + BusVelocity * ((WindowStart + WindowEnd) * 0.5 - BusStartTime);
    double BestJump = (WindowStart + WindowEnd) * 0.5;
    double BestGap = -1.0;
    size_t BestPoi = (size_t)-1;

    for (int Try = 0; Try < 24; Try++)
    {
        FVector Target;
        double JumpAt = 0.0;
        size_t Poi = (size_t)-1;

        if (bUsePoi)
        {
            Poi = Candidates[RandInt(0, (int)Candidates.size() - 1)];
            for (auto Index : Candidates)
                if (DropSpotUsage[Index] < DropSpotUsage[Poi])
                    Poi = Index;

            Target = DropSpots[Poi];
            Target.X += RandRange(-7000.f, 7000.f);
            Target.Y += RandRange(-7000.f, 7000.f);

            auto Offset = Target - BusStartLocation;
            JumpAt = BusStartTime + (Offset.X * Direction.X + Offset.Y * Direction.Y) / Speed;
        }
        else
        {
            double U = RandRange(0.f, 1.f);
            double Lateral = RandRange(-1.f, 1.f) * MaxLateral;
            double Edge = 2.0 * U - 1.0;
            double Width = (std::max)(0.35, sqrt((std::max)(0.0, 1.0 - Edge * Edge)));
            if (fabs(Lateral) > MaxLateral * Width)
                continue;

            JumpAt = WindowStart + U * (WindowEnd - WindowStart);
            Target = BusStartLocation + BusVelocity * (JumpAt - BusStartTime) + Right * Lateral;
        }

        double Gap = 1e18;
        for (auto& Taken : JumpTargets)
            Gap = (std::min)(Gap, Dist2D(Taken, Target));

        if (Gap > BestGap)
        {
            BestGap = Gap;
            BestTarget = Target;
            BestJump = JumpAt;
            BestPoi = Poi;
        }

        if (Gap >= MinSeparation)
            break;
    }

    if (BestPoi != (size_t)-1)
        DropSpotUsage[BestPoi]++;

    JumpTargets.push_back(BestTarget);
    Bot.LandingTarget = BestTarget;

    double JumpLow = DropStart + 0.5;
    double JumpHigh = (std::max)(DropEnd - 1.0, JumpLow);
    Bot.JumpTime = std::clamp(BestJump + (double)RandRange(-0.5f, 0.5f), JumpLow, JumpHigh);

    if (Bot.JumpTime < Time)
        Bot.JumpTime = Time + RandRange(0.f, 2.f);
}

static void JumpFromBus(FBot& Bot, ABotController* Controller, ABotPawn* Pawn, FVector Location)
{
    Location.X += RandRange(-500.f, 500.f);
    Location.Y += RandRange(-500.f, 500.f);
    Location.Z -= 600.f;

    Pawn->K2_SetActorLocation(Location, false, nullptr, true);
    Pawn->SetActorHiddenInGame(false);
    Pawn->SetActorEnableCollision(true);
    if (auto Movement = Pawn->CharacterMovement)
        Movement->SetMovementMode((uint8)3 , (uint8)0);
    Pawn->BeginSkydiving(true);

    SetBlackboardBool(Controller, L"AIEvaluator_Global_IsInBus", false);
    SetBlackboardBool(Controller, L"AIEvaluator_Global_HasEverJumpedFromBusKey", true);
    SetBlackboardVector(Controller, L"AIEvaluator_JumpOffBus_Destination", Bot.LandingTarget);

    Bot.State = EBotState::Skydiving;
    Bot.SkydiveStartTime = Now();
    Bot.LastMoveTime = 0;
}

static bool ThankedFlag(AFortPlayerStateAthena* PlayerState, bool bSet)
{
    static int32 Offset = -2;
    static uint8 Mask = 0;
    if (Offset == -2)
    {
        auto Prop = PlayerState->GetProperty("bThankedBusDriver", 0x20000);
        Offset = Prop ? (int32)GetFromOffset<uint32>(Prop, Offsets::Offset_Internal) : -1;
        Mask = Prop ? Prop->GetFieldMask() : 0;
    }
    if (Offset < 0 || !Mask)
        return false;

    auto Byte = (uint8*)PlayerState + Offset;
    if (bSet)
        *Byte |= Mask;
    return (*Byte & Mask) != 0;
}

static bool CallThankBusDriver(ABotController* Controller)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        Controller->ThankBusDriver();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;
    return bOk;
}

static void ThankBusDriver(ABotController* Controller)
{
    static int Thanked = 0, ByGame = 0, ByHand = 0;
    static bool bNativeFaulted = false;

    auto PlayerState = Controller->PlayerState;
    if (!PlayerState)
        return;

    if (!bNativeFaulted && !CallThankBusDriver(Controller))
    {
        bNativeFaulted = true;
        BotLog("[Bots] The game's ThankBusDriver faulted for a bot, setting the flag by hand from now on.\n");
    }

    if (ThankedFlag(PlayerState, false))
        ByGame++;
    else if (ThankedFlag(PlayerState, true))
    {
        ByHand++;
        if (auto OnRep = PlayerState->GetFunction("OnRep_ThankedBusDriver"))
            PlayerState->ProcessEvent(OnRep, nullptr);
    }
    else
    {
        static bool bWarned = false;
        if (!bWarned)
        {
            bWarned = true;
            BotLog("[Bots] This build's player state has no bThankedBusDriver, bots can't thank the bus driver.\n");
        }
        return;
    }

    PlayerState->ForceNetUpdate();
    if (++Thanked <= 3 || (Thanked % 25) == 0)
        BotLog("[Bots] %d bots thanked the bus driver so far (%d by the game's own call, %d by setting the flag).\n", Thanked, ByGame, ByHand);
}

static void PlanThanks(FBot& Bot, double Time)
{
    if (RandRange(0.f, 1.f) >= FConfiguration::BotThankBusDriverChance)
    {
        Bot.ThankTime = -2;
        return;
    }

    double Earliest = (std::max)(BusStartTime + 1.5, Time);
    double Latest = (std::min)(BusStartTime + 15.0, Bot.JumpTime - 0.3);
    Bot.ThankTime = Latest > Earliest ? Earliest + RandRange(0.f, 1.f) * (Latest - Earliest) : Time;
}

static void TickBus(double Time)
{
    auto Aircraft = GetAircraft();
    if (!Aircraft)
    {
        if ((EAthenaGamePhase)GetGamePhase() < EAthenaGamePhase::SafeZones)
            return;

        for (auto& Bot : AllBots)
        {
            auto Controller = GetWeak<ABotController>(Bot.Controller);
            auto Pawn = GetWeak<ABotPawn>(Bot.Pawn);
            if (Bot.State != EBotState::OnBus || !Controller || !Pawn)
                continue;

            FVector Location = bBusVelocityKnown ? BusStartLocation + BusVelocity * (Time - BusStartTime) : Pawn->K2_GetActorLocation() + FVector(0, 0, 15000);
            if (BusStartTime >= 0)
                Location.Z = BusStartLocation.Z;
            Bot.LandingTarget = Location;
            JumpFromBus(Bot, Controller, Pawn, Location);
        }
        return;
    }

    auto BusLocation = Aircraft->K2_GetActorLocation();
    if (BusStartTime < 0)
    {
        BusStartTime = Time;
        BusStartLocation = BusLocation;
        return;
    }

    if (!bBusVelocityKnown)
    {
        if (Time - BusStartTime < 1.0)
            return;

        BusVelocity = (BusLocation - BusStartLocation) / (Time - BusStartTime);
        BusVelocity.Z = 0;
        if (BusVelocity.Magnitude() < 1.0)
            return;
        bBusVelocityKnown = true;
    }

    for (auto& Bot : AllBots)
    {
        auto Controller = GetWeak<ABotController>(Bot.Controller);
        auto Pawn = GetWeak<ABotPawn>(Bot.Pawn);
        if (Bot.State != EBotState::OnBus || !Controller || !Pawn)
            continue;

        if (Bot.JumpTime < 0)
            PlanJump(Bot, Aircraft, Time);

        bool bJumpNow = Time >= Bot.JumpTime || (Aircraft->DropEndTime > 0 && Time >= Aircraft->DropEndTime - 0.5);

        if (Bot.ThankTime == -1)
            PlanThanks(Bot, Time);
        if (Bot.ThankTime >= 0 && (Time >= Bot.ThankTime || bJumpNow))
        {
            Bot.ThankTime = -2;
            ThankBusDriver(Controller);
        }

        if (bJumpNow)
            JumpFromBus(Bot, Controller, Pawn, BusLocation);
    }

    if (!bLoggedJumpSpread && JumpTargets.size() > 1)
    {
        bool bAllPlanned = true;
        for (auto& Bot : AllBots)
            if (Bot.State == EBotState::OnBus && Bot.JumpTime < 0 && GetWeak<ABotController>(Bot.Controller) && GetWeak<ABotPawn>(Bot.Pawn))
                bAllPlanned = false;

        if (bAllPlanned)
        {
            bLoggedJumpSpread = true;

            double Sum = 0.0, Tightest = 1e18;
            for (size_t i = 0; i < JumpTargets.size(); i++)
            {
                double Nearest = 1e18;
                for (size_t j = 0; j < JumpTargets.size(); j++)
                    if (i != j)
                        Nearest = (std::min)(Nearest, Dist2D(JumpTargets[i], JumpTargets[j]));
                Sum += Nearest;
                Tightest = (std::min)(Tightest, Nearest);
            }

            BotLog("[Bots] Drop plan: %d landing spots across the whole route, avg gap to nearest bot %.0fm, tightest pair %.0fm (%d POIs found on this map).\n",
                   (int)JumpTargets.size(), Sum / (double)JumpTargets.size() / 100.0, Tightest / 100.0, (int)DropSpots.size());
        }
    }
}

static void MakeBotDamageable(ABotPawn* Pawn, AFortPlayerStateAthena* PlayerState);

static void TickSkydive(FBot& Bot, ABotController* Controller, ABotPawn* Pawn, double Time)
{
    auto Location = Pawn->K2_GetActorLocation();
    auto Movement = (UBotMovement*)Pawn->CharacterMovement;

    double Delta = Bot.LastMoveTime > 0 ? std::clamp(Time - Bot.LastMoveTime, 0.0, 0.1) : 0.0;
    Bot.LastMoveTime = Time;
    if (fabs(Location.Z - Bot.LastSkydiveZ) < 5.0)
        Bot.SkydiveStillTime += Delta;
    else
        Bot.SkydiveStillTime = 0;
    Bot.LastSkydiveZ = Location.Z;

    bool bLanded = (!Pawn->bIsSkydiving && Movement && Movement->IsMovingOnGround()) || (Time - Bot.SkydiveStartTime > 5.0 && Bot.SkydiveStillTime > 1.0);
    if (bLanded || Time - Bot.SkydiveStartTime > 90.0)
    {
        Bot.State = EBotState::Playing;
        SetBlackboardBool(Controller, L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey", true);
        UpdatePhaseKeys(Controller);
        StartBehaviorTree(Controller);
        MakeBotDamageable(Pawn, GetWeak<AFortPlayerStateAthena>(Bot.PlayerState));

        if (!Bot.bSeasonLoadoutGiven)
        {
            Bot.bSeasonLoadoutGiven = true;
            FBotSetupContext Context{ nullptr, Controller, Pawn, nullptr };
            GuardedStep("season loadout", [](FBotSetupContext& C) { GiveSeasonLoadout(C.Controller, C.Pawn); }, Context, false);
        }
        return;
    }

    auto Direction = Bot.LandingTarget - Location;
    Direction.Z = 0;
    double Distance = Direction.Magnitude();
    if (Distance < 800.0)
        return;

    Direction = Direction / Distance;
    Pawn->AddMovementInput(Direction, 1.f, true);
    Controller->SetControlRotation(MakeRotator(0, atan2(Direction.Y, Direction.X) * 57.29577951308232));

    Bot.LastMoveTime = Time;
    if (Delta <= 0.0 || !Movement)
        return;

    auto Velocity = Movement->Velocity;
    Velocity.Z = 0;
    if (Velocity.Magnitude() < 150.0)
        Pawn->K2_SetActorLocation(Location + Direction * (1200.0 * Delta), true, nullptr, false);
}

static void TickStorm(FBot& Bot, ABotPawn* Pawn, double Time)
{
    if (!FConfiguration::bBotStormDamage || Time < Bot.NextStormDamageTime)
        return;

    Bot.NextStormDamageTime = Time + 1.0;

    auto SafeZoneIndicator = GetSafeZoneIndicator();
    if (!SafeZoneIndicator)
        return;

    bool bInZone = IsInSafeZone(Pawn->K2_GetActorLocation());
    if (Pawn->bIsInsideSafeZone != bInZone || Pawn->bIsInAnyStorm != !bInZone)
    {
        Pawn->bIsInAnyStorm = !bInZone;
        Pawn->OnRep_IsInAnyStorm();
        Pawn->bIsInsideSafeZone = bInZone;
        Pawn->OnRep_IsInsideSafeZone();
    }

    if (bInZone)
        return;

    float Damage = 1.f;
    if (SafeZoneIndicator->HasCurrentDamageInfo())
    {
        auto& DamageInfo = SafeZoneIndicator->CurrentDamageInfo;
        Damage = DamageInfo.Damage;
        if (DamageInfo.bPercentageBasedDamage)
            Damage *= 100.f;
    }
    if (Damage <= 0.f)
        Damage = 1.f;

    auto Health = Pawn->GetHealth() - Damage;
    if (Health <= 0.f)
        Pawn->ForceKill(FGameplayTag(), (AActor*)nullptr, (AActor*)nullptr);
    else
        Pawn->SetHealth(Health);
}

static AActor* FindKiller(FBot& Bot, ABotPawn* Pawn)
{
    if (Pawn && Pawn->HasDamagers())
    {
        auto& Damagers = Pawn->Damagers;
        for (int i = Damagers.Num() - 1; i >= 0; i--)
        {
            auto Causer = Damagers[i].DamageCauser;
            if (Causer && Causer != Bot.Controller.Get() && GetControllerPlayerState(Causer))
                return Causer;
        }
    }

    return GetWeak(Bot.LastAttacker);
}

static void DropInventory(ABotController* Controller, ABotPawn* Pawn, const FVector& Location)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory)
        return;

    int32 Slots = 0, AmmoStacks = 0, AmmoTotal = 0, Other = 0;
    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        auto Definition = Entry.ItemDefinition;
        if (!Definition || Entry.Count <= 0 || Definition->IsA<UFortWeaponMeleeItemDefinition>() || !Definition->CanBeDropped())
            continue;

        int32 Count = Entry.Count;
        if (IsAmmo(Definition))
        {
            Count = (std::min)(Count, BotAmmoCap(Definition));
            AmmoStacks++;
            AmmoTotal += Count;
        }
        else if (TakesSlot(Definition))
        {
            if (Slots >= BotSlotCount)
                continue;
            Slots++;
        }
        else
            Other++;

        AFortInventory::SpawnPickup(Location, Definition, Count, Entry.LoadedAmmo, EFortPickupSourceTypeFlag::GetPlayer(), EFortPickupSpawnSource::GetPlayerElimination(), Pawn);
    }

    static int Reported = 0;
    if (++Reported <= 10)
        BotLog("[Bots] An eliminated bot dropped %d slot item(s) (max %d), %d ammo stack(s) totalling %d rounds, %d other item(s).\n", Slots, BotSlotCount, AmmoStacks,
               AmmoTotal, Other);
}

uint8 ToDeathCause(AFortPlayerPawnAthena* Pawn, FGameplayTagContainer& DeathTags, bool bDBNO);

static void HandleElimination(FBot& Bot, ABotPawn* Pawn)
{
    auto GameMode = GetGameMode();
    auto GameState = GameMode->GameState;
    auto Controller = GetWeak<ABotController>(Bot.Controller);
    auto PlayerState = GetWeak<AFortPlayerStateAthena>(Bot.PlayerState);
    auto KillerController = FindKiller(Bot, Pawn);
    auto KillerPlayerState = KillerController ? GetControllerPlayerState(KillerController) : nullptr;
    auto DeathLocation = Pawn ? Pawn->K2_GetActorLocation() : FVector();

    DropInventory(Controller, Pawn, DeathLocation);

    if (!PlayerState)
        return;

    if (PlayerState->HasPlace())
    {
        PlayerState->Place = GameState->PlayersLeft;
        PlayerState->OnRep_Place();
    }

    auto KillerPawn = KillerController ? (AFortPlayerPawnAthena*)((ABotController*)KillerController)->Pawn : nullptr;

    uint8 DeathCause = 0;
    if (PlayerState->HasDeathInfo())
    {
        memset(&PlayerState->DeathInfo, 0, FDeathInfo::Size());
        SetDeathInfoActor(PlayerState->DeathInfo, "Killer", KillerPlayerState);
        if (FDeathInfo::HasDeathLocation())
            PlayerState->DeathInfo.DeathLocation = DeathLocation;
        if (FDeathInfo::HasDeathTags() && Pawn)
            PlayerState->DeathInfo.DeathTags =
                *(FGameplayTagContainer*)(__int64(&Pawn->MoveSoundStimulusBroadcastInterval) + (VersionInfo.FortniteVersion >= 11 && VersionInfo.FortniteVersion < 18 ? 0x18 : 0x10));
        if (FDeathInfo::HasDeathClassSlot())
            PlayerState->DeathInfo.DeathClassSlot = -1;
        if (Pawn)
            PlayerState->DeathInfo.DeathCause = DeathCause = ToDeathCause(Pawn, PlayerState->DeathInfo.DeathTags, false);
        SetDeathInfoActor(PlayerState->DeathInfo, "FinisherOrDowner", KillerPlayerState ? KillerPlayerState : PlayerState);
        if (FDeathInfo::HasFinisherOrDownerTags() && (KillerPawn || Pawn))
            PlayerState->DeathInfo.FinisherOrDownerTags = KillerPawn ? KillerPawn->GameplayTags : Pawn->GameplayTags;
        if (FDeathInfo::HasVictimTags() && Pawn)
            PlayerState->DeathInfo.VictimTags = Pawn->GameplayTags;
        if (FDeathInfo::HasDistance() && KillerPawn)
            PlayerState->DeathInfo.Distance = (float)(KillerPawn->K2_GetActorLocation() - DeathLocation).Magnitude();
        SetDeathInfoTime(PlayerState->DeathInfo);
        if (FDeathInfo::HasbInitialized())
            PlayerState->DeathInfo.bInitialized = true;
        PlayerState->OnRep_DeathInfo();
        PlayerState->ForceNetUpdate();
    }

    static int FeedReports = 0;
    if (++FeedReports <= 10)
        BotLog("[Bots] Elimination for the kill feed: %s -> a bot (cause %d, killer written %s, time written %s).\n",
               KillerPlayerState && KillerPlayerState != PlayerState ? (KillerController && KillerController->IsA<AFortPlayerControllerAthena>() ? "a player" : "a bot") : "nobody",
               (int)DeathCause, FDeathInfo::StaticStruct() && (FDeathInfo::StaticStruct()->GetProperty("FinisherOrDowner", 0x8000000) || FDeathInfo::HasFinisherOrDowner()) ? "yes" : "NO",
               FDeathInfo::HasDeathServerTime() ? "yes" : "no field on this build");

    if (!KillerPlayerState || KillerPlayerState == PlayerState)
        return;

    if (KillerPlayerState->HasKillScore())
        KillerPlayerState->KillScore++;
    else
        KillerPlayerState->Kills++;
    KillerPlayerState->OnRep_Kills();
    if (KillerPlayerState->HasTeamKillScore())
    {
        KillerPlayerState->TeamKillScore++;
        KillerPlayerState->OnRep_TeamKillScore();
    }

    struct
    {
        AFortPlayerStateAthena* PlayerState;
        uint8_t Padding[0x8];
    } KillReport{ PlayerState };
    KillerPlayerState->ClientReportKill(KillReport);
    if (KillerPlayerState->HasTeamKillScore())
        KillerPlayerState->ClientReportTeamKill(KillerPlayerState->TeamKillScore);

    if (FConfiguration::SiphonAmount > 0 && KillerPawn && KillerController->IsA<AFortPlayerControllerAthena>())
    {
        auto Health = KillerPawn->GetHealth() + FConfiguration::SiphonAmount;
        auto Shield = KillerPawn->GetShield();
        if (Health > 100.f)
        {
            Shield = (std::min)(Shield + (Health - 100.f), 100.f);
            Health = 100.f;
        }
        KillerPawn->SetHealth(Health);
        KillerPawn->SetShield(Shield);
    }
}

static void GuardedHandleElimination(FBot& Bot, ABotPawn* Pawn)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        HandleElimination(Bot, Pawn);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
    {
        static int Faults = 0;
        if (++Faults <= 5)
            BotLog("[Bots] Handling a bot's elimination faulted, skipped the rest of it (%d so far).\n", Faults);
    }
}

static uint8 GetTeam(const FBot& Bot)
{
    auto PlayerState = GetWeak<AFortPlayerStateAthena>(Bot.PlayerState);
    return PlayerState ? PlayerState->TeamIndex : 0;
}

static void DeclareWinner(AFortGameMode* GameMode, uint8 Team)
{
    auto GameState = GameMode->GameState;
    bDeclaredWinner = true;

    for (auto& Player__Uncasted : GameMode->AlivePlayers)
    {
        auto Player = (AFortPlayerControllerAthena*)Player__Uncasted;
        auto PlayerState = Player ? Player->PlayerState : nullptr;
        if (!PlayerState || PlayerState->TeamIndex != Team)
            continue;

        auto Pawn = Player->MyFortPawn;

        if (PlayerState->HasPlace())
        {
            PlayerState->Place = 1;
            PlayerState->OnRep_Place();
        }

        if (VersionInfo.FortniteVersion >= 16)
            Player->PlayWinEffects(Pawn, (UFortWeaponItemDefinition*)nullptr, (uint8)0, false);
        Player->ClientNotifyWon(Pawn, (UFortWeaponItemDefinition*)nullptr, (uint8)0);
        Player->ClientNotifyTeamWon(Pawn, (UFortWeaponItemDefinition*)nullptr, (uint8)0);

        if (VersionInfo.FortniteVersion >= 19 && Player->WorldInventory)
        {
            if (auto Crown = FindObject<UFortItemDefinition>(L"/VictoryCrownsGameplay/Items/AGID_VictoryCrown.AGID_VictoryCrown"))
            {
                TArray<FFortItemEntryStateValue> StateValues{};
                auto Value = (FFortItemEntryStateValue*)malloc(FFortItemEntryStateValue::Size());
                memset((PBYTE)Value, 0, FFortItemEntryStateValue::Size());
                Value->IntValue = 1;
                Value->StateType = 2;
                StateValues.Add(*Value, FFortItemEntryStateValue::Size());
                free(Value);
                Player->WorldInventory->GiveItem(Crown, 1, 0, 0, true, true, 0, StateValues);
                StateValues.Free();
            }
        }

        if (GameState->HasWinningPlayerState())
        {
            GameState->WinningPlayerState = PlayerState;
            GameState->OnRep_WinningPlayerState();
        }
    }

    GameState->WinningTeam = Team;
    GameState->OnRep_WinningTeam();
    BotLog("[Bots] Team %d won the match.\n", Team);
}

static void CheckForWinner(AFortGameMode* GameMode, uint8 GamePhase)
{
    if (bDeclaredWinner || SpawnedBots == 0 || (EAthenaGamePhase)GamePhase < EAthenaGamePhase::SafeZones)
        return;

    auto GameState = GameMode->GameState;
    if (GameState->HasWinningPlayerState() && GameState->WinningPlayerState)
    {
        bDeclaredWinner = true;
        return;
    }

    std::vector<uint8> AliveTeams;
    for (auto& Bot : AllBots)
    {
        if (Bot.State == EBotState::Dead)
            continue;
        auto Team = GetTeam(Bot);
        if (std::find(AliveTeams.begin(), AliveTeams.end(), Team) == AliveTeams.end())
            AliveTeams.push_back(Team);
    }

    bool bAnyBotTeam = !AliveTeams.empty();
    for (auto& Player : GameMode->AlivePlayers)
    {
        auto PlayerController = (AFortPlayerControllerAthena*)Player;
        if (!PlayerController || !PlayerController->PlayerState)
            continue;
        auto Team = PlayerController->PlayerState->TeamIndex;
        if (std::find(AliveTeams.begin(), AliveTeams.end(), Team) == AliveTeams.end())
            AliveTeams.push_back(Team);
    }

    if (AliveTeams.size() == 1 && !bAnyBotTeam)
        DeclareWinner(GameMode, AliveTeams[0]);
}

static FFortRangedWeaponStats* GetGunStats(const UFortItemDefinition* Definition)
{
    if (!IsRangedWeapon(Definition))
        return nullptr;
    return AFortInventory::GetStats((const UFortWeaponItemDefinition*)Definition);
}

static bool NeedsServerDamage(FFortRangedWeaponStats* Stats)
{
    if (!Stats || !Stats->HasDmgPB() || Stats->DmgPB <= 0.f)
        return false;
    return !(Stats->HasBulletsPerCartridge() && Stats->BulletsPerCartridge > 1);
}

static FBot* FindBotByPawn(const AActor* Pawn)
{
    for (auto& Bot : AllBots)
        if (Bot.State != EBotState::Dead && Bot.Pawn.Get() == Pawn)
            return &Bot;
    return nullptr;
}

static void TickBotShots(FBot& Bot, ABotController* Controller, ABotPawn* Pawn, ABotPawn* Target, double Distance, double Time)
{
    if (!FConfiguration::bBotGunDamage || !Bot.bFiring || Time < Bot.NextShotTime)
        return;

    static auto RangedWeaponClass = AFortWeaponRanged::StaticClass();
    auto Weapon = (AFortWeaponRanged*)Pawn->CurrentWeapon;
    if (!Weapon || !RangedWeaponClass || !Weapon->IsA(RangedWeaponClass))
        return;

    auto Stats = GetGunStats(Weapon->WeaponData);
    if (!NeedsServerDamage(Stats))
        return;

    float FireRate = Stats->HasFiringRate() && Stats->FiringRate > 0.1f ? Stats->FiringRate : 4.f;
    Bot.NextShotTime = Time + 1.0 / FireRate;

    if (!Controller->LineOfSightTo(Target, FVector(), false))
        return;

    float HitChance = std::clamp(0.02f + FConfiguration::BotAccuracy - (float)(Distance / 40000.0), 0.01f, 0.2f);
    if (RandRange(0.f, 1.f) > HitChance)
        return;

     (void)0;
}

static TArray<AActor*> CachedPickups;
static TArray<AActor*> CachedContainers;
static double NextLootRefreshTime = 0;

static void RefreshLootCache(double Time)
{
    if (Time < NextLootRefreshTime)
        return;
    NextLootRefreshTime = Time + 5.0;

    CachedPickups.Free();
    CachedContainers.Free();
    CachedPickups = GetActorsOfClass(AFortPickupAthena::StaticClass());
    CachedContainers = GetActorsOfClass(FindClass("BuildingContainer"));
}

static void SetGoal(FBot& Bot, FVector Goal, double Time)
{
    Bot.MoveGoal = Goal;
    Bot.bHasMoveGoal = true;
    Bot.MoveGoalTime = Time;
    Bot.StuckCount = 0;
}

static AActor* FindLoot(ABotController* Controller, ABotPawn* Pawn, double Range)
{
    auto Location = Pawn->K2_GetActorLocation();
    AActor* Best = nullptr;
    double BestDistance = Range;

    for (auto& Actor : CachedPickups)
    {
        auto Pickup = (AFortPickupAthena*)Actor;
        if (!Pickup || Pickup->bActorIsBeingDestroyed || Pickup->bPickedUp || !IsGun(Pickup->PrimaryPickupItemEntry.ItemDefinition) ||
            !BotWantsItem(Controller, Pickup->PrimaryPickupItemEntry.ItemDefinition))
            continue;
        auto Distance = (Pickup->K2_GetActorLocation() - Location).Magnitude();
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Best = Pickup;
        }
    }

    for (auto& Actor : CachedContainers)
    {
        auto Container = (ABuildingContainer*)Actor;
        if (!Container || Container->bActorIsBeingDestroyed || Container->bAlreadySearched)
            continue;
        auto Distance = (Container->K2_GetActorLocation() - Location).Magnitude();
        if (Distance < BestDistance * 0.8)
        {
            BestDistance = Distance;
            Best = Container;
        }
    }

    return Best;
}

static AActor* FindNearestHuman(AFortGameMode* GameMode, const FVector& Location, double& OutDistance)
{
    AActor* Best = nullptr;
    OutDistance = 1e12;
    for (auto& Player : GameMode->AlivePlayers)
    {
        auto PlayerController = (AFortPlayerControllerAthena*)Player;
        auto Pawn = PlayerController ? (AActor*)PlayerController->MyFortPawn : nullptr;
        if (!Pawn)
            continue;
        auto Distance = (Pawn->K2_GetActorLocation() - Location).Magnitude();
        if (Distance < OutDistance)
        {
            OutDistance = Distance;
            Best = Pawn;
        }
    }
    return Best;
}

static bool IsHoldingGun(ABotPawn* Pawn)
{
    auto Weapon = (AFortWeapon*)Pawn->CurrentWeapon;
    return Weapon && Weapon->WeaponData && IsGun(Weapon->WeaponData);
}

static void EquipGunIfHoldingPickaxe(ABotController* Controller, ABotPawn* Pawn)
{
    if (IsHoldingGun(Pawn) || !Controller->Inventory)
        return;

    FFortItemEntry* PickaxeEntry = nullptr;
    auto& Entries = Controller->Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (IsGun(Entry.ItemDefinition))
        {
            EquipEntry(Pawn, &Entry);
            return;
        }
        if (!PickaxeEntry && Entry.ItemDefinition && Entry.ItemDefinition->IsA<UFortWeaponMeleeItemDefinition>())
            PickaxeEntry = &Entry;
    }

    auto Weapon = (AFortWeapon*)Pawn->CurrentWeapon;
    bool bHoldingPickaxe = Weapon && Weapon->WeaponData && Weapon->WeaponData->IsA<UFortWeaponMeleeItemDefinition>();
    if (PickaxeEntry && !bHoldingPickaxe)
        EquipEntry(Pawn, PickaxeEntry);
}

static void TryGrabLoot(FBot& Bot, ABotController* Controller, ABotPawn* Pawn)
{
    auto Target = GetWeak(Bot.LootTarget);
    if (!Target || (Target->K2_GetActorLocation() - Pawn->K2_GetActorLocation()).Magnitude() > 350.0)
        return;

    if (Target->IsA<AFortPickupAthena>())
    {
        auto Pickup = (AFortPickupAthena*)Target;
        if (!Pickup->bPickedUp && BotWantsItem(Controller, Pickup->PrimaryPickupItemEntry.ItemDefinition))
            Pawn->ServerHandlePickup(Pickup, 0.4f, FVector(), true);
    }
    else if (!((ABuildingContainer*)Target)->bAlreadySearched)
        UFortLootPackage::SpawnLootHook((ABuildingContainer*)Target);

    Bot.LootTarget = FWeakObjectPtr();
    Bot.bHasMoveGoal = false;
    NextLootRefreshTime = 0;
}

static void TickDirector(AFortGameMode* GameMode, FBot& Bot, ABotController* Controller, ABotPawn* Pawn, uint8 GamePhase, double Time)
{
    if (Time < Bot.NextDirectorTime)
        return;
    Bot.NextDirectorTime = Time + 2.0 + RandRange(0.f, 1.f);

    auto Location = Pawn->K2_GetActorLocation();
    bool bIdle = Dist2D(Location, Bot.LastDirectorLocation) < 250.0;
    Bot.IdleChecks = bIdle ? Bot.IdleChecks + 1 : 0;
    Bot.LastDirectorLocation = Location;

    EquipGunIfHoldingPickaxe(Controller, Pawn);

    Bot.bRotating = false;
    if ((EAthenaGamePhase)GamePhase >= EAthenaGamePhase::SafeZones)
    {
        auto SafeZone = GetSafeZoneIndicator();
        if (SafeZone && SafeZone->NextRadius > 0.f)
        {
            auto Center = SafeZone->NextCenter;
            double Radius = SafeZone->NextRadius;
            double Inner = Radius < 15000.0 ? 0.35 : 0.6;
            double Distance = Dist2D(Location, Center);
            bool bInStorm = !IsInSafeZone(Location);

            if (bInStorm || Distance > Radius * (Inner + 0.15))
            {
                Bot.bRotating = true;
                if (Time >= Bot.DetourUntil && (!Bot.bHasMoveGoal || Dist2D(Bot.MoveGoal, Center) > Radius * Inner))
                {
                    float Spread = (float)Inner * 0.7f;
                    auto Goal = Center + FVector(RandRange(-Spread, Spread) * Radius, RandRange(-Spread, Spread) * Radius, 0);
                    Goal.Z = Location.Z;
                    SetGoal(Bot, Goal, Time);
                }
                return;
            }
        }
    }

    bool bArmed = HasRangedWeapon(Controller);
    {
        RefreshLootCache(Time);
        auto Target = GetWeak(Bot.LootTarget);
        if (!Target || Time - Bot.LootTargetTime > 25.0 || Bot.IdleChecks >= 4)
        {
            Target = FindLoot(Controller, Pawn, bArmed ? 2500.0 : 8000.0);
            Bot.LootTarget = FWeakObjectPtr(Target);
            Bot.LootTargetTime = Time;
            Bot.IdleChecks = 0;
        }
        if (Target)
        {
            SetGoal(Bot, Target->K2_GetActorLocation(), Time);
            return;
        }
    }

    double HumanDistance = 0;
    auto Human = (EAthenaGamePhase)GamePhase >= EAthenaGamePhase::SafeZones ? FindNearestHuman(GameMode, Location, HumanDistance) : nullptr;
    double Pull = FConfiguration::BotPlayerAttraction;
    if (Human && HumanDistance > 1500.0 &&
        ((!Bot.bHasMoveGoal && RandRange(0.f, 1.f) < Pull) || (HumanDistance > 25000.0 && RandRange(0.f, 1.f) < Pull * 0.25f)))
    {
        SetGoal(Bot, Human->K2_GetActorLocation() + FVector(RandRange(-2500.f, 2500.f), RandRange(-2500.f, 2500.f), 0), Time);
        return;
    }

    if (!Bot.bHasMoveGoal || Time - Bot.MoveGoalTime > 20.0)
    {
        float Angle = RandRange(0.f, 6.2831853f);
        float Distance = RandRange(1500.f, 4000.f);
        SetGoal(Bot, Location + FVector(cos(Angle) * Distance, sin(Angle) * Distance, 0), Time);
    }
}

struct FCombatant
{
    ABotPawn* Pawn;
    FWeakObjectPtr Weak;
    uint8 Team;
    FVector Location;
};
static std::vector<FCombatant> Combatants;
static double NextCombatantsRefresh = 0;

static void RefreshCombatants(AFortGameMode* GameMode, double Time)
{
    if (Time < NextCombatantsRefresh)
        return;
    NextCombatantsRefresh = Time + 0.25;
    Combatants.clear();

    auto Add = [&](ABotPawn* Other, AFortPlayerStateAthena* OtherState) {
        if (!Other || IsPawnDead(Other) || (Other->HasbIsSkydiving() && Other->bIsSkydiving))
            return;
        Combatants.push_back({ Other, FWeakObjectPtr(Other), OtherState ? OtherState->TeamIndex : (uint8)255, Other->K2_GetActorLocation() });
    };

    for (auto& Player : GameMode->AlivePlayers)
        if (auto PlayerController = (AFortPlayerControllerAthena*)Player)
            Add((ABotPawn*)PlayerController->MyFortPawn, PlayerController->PlayerState);

    for (auto& Other : AllBots)
        if (Other.State == EBotState::Playing)
            Add(GetWeak<ABotPawn>(Other.Pawn), GetWeak<AFortPlayerStateAthena>(Other.PlayerState));
}

static int32 DamagerStride()
{
    static int32 Stride = -1;
    if (Stride == -1)
    {
        auto Struct = FindStruct("DamagerInfo");
        Stride = Struct ? Struct->GetPropertiesSize() : (int32)sizeof(FDamagerInfo);
        if (Stride < (int32)sizeof(AActor*) || Stride > 0x200)
            Stride = (int32)sizeof(FDamagerInfo);
    }
    return Stride;
}

static ABotPawn* LivePawnOf(AActor* Causer)
{
    if (!Causer)
        return nullptr;

    ABotPawn* Pawn = nullptr;
    if (IsController(Causer))
        Pawn = (ABotPawn*)((ABotController*)Causer)->Pawn;
    else if (Causer->IsA<AFortPlayerPawnAthena>())
        Pawn = (ABotPawn*)Causer;
    return Pawn && !IsPawnDead(Pawn) ? Pawn : nullptr;
}

static ABotPawn* RecentAttacker(FBot& Bot, ABotPawn* Pawn, double Time)
{
    if (Time - Bot.LastHurtTime > 5.0)
        return nullptr;

    if (Pawn->HasDamagers())
    {
        auto& Damagers = Pawn->Damagers;
        auto Stride = DamagerStride();
        for (int i = Damagers.Num() - 1; i >= 0 && i >= Damagers.Num() - 8; i--)
        {
            auto Causer = Damagers.Get(i, Stride).DamageCauser;
            if (!Causer || Causer == Bot.Controller.Get())
                continue;
            auto Other = LivePawnOf(Causer);
            if (Other && Other != Pawn)
                return Other;
        }
    }

    auto Other = LivePawnOf(GetWeak(Bot.LastAttacker));
    return Other != Pawn ? Other : nullptr;
}

static AActor* FindEnemy(AFortGameMode* GameMode, FBot& Bot, ABotController* Controller, ABotPawn* Pawn, double Range, ABotPawn* Attacker)
{
    auto MyState = GetWeak<AFortPlayerStateAthena>(Bot.PlayerState);
    auto MyTeam = MyState ? MyState->TeamIndex : 255;
    auto Location = Pawn->K2_GetActorLocation();

    if (Attacker && (Attacker->K2_GetActorLocation() - Location).Magnitude() < Range * 1.5 && Controller->LineOfSightTo(Attacker, FVector(), false))
        return Attacker;

    std::vector<std::pair<double, const FCombatant*>> InRange;
    for (auto& Combatant : Combatants)
    {
        if (Combatant.Pawn == Pawn || Combatant.Team == MyTeam || Combatant.Pawn == Attacker)
            continue;
        double Distance = (Combatant.Location - Location).Magnitude();
        if (Distance < Range)
            InRange.push_back({ Distance, &Combatant });
    }

    std::sort(InRange.begin(), InRange.end(), [](auto& A, auto& B) { return A.first < B.first; });
    int Checked = 0;
    for (auto& [Distance, Combatant] : InRange)
    {
        auto Other = GetWeak<ABotPawn>(Combatant->Weak);
        if (!Other)
            continue;
        if (Controller->LineOfSightTo(Other, FVector(), false))
            return Other;
        if (++Checked >= 3)
            break;
    }

    return nullptr;
}

static bool GuardedStartFire(ABotPawn* Pawn)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        Pawn->PawnStartFire((uint8)0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
    {
        static int Faults = 0;
        if (++Faults <= 5)
            BotLog("[Bots] A bot's trigger pull faulted inside the engine, that bot stops shooting.\n");
    }
    return bOk;
}

static void StopFiring(FBot& Bot, ABotPawn* Pawn)
{
    if (Bot.bFiring)
        Pawn->PawnStopFire((uint8)0);
    Bot.bFiring = false;
}

class ABotWeapon : public AFortWeaponRanged
{
public:
    DEFINE_FUNC(Reload, void);
};

static int32 GReloadsAsked = 0;
static int32 GReloadsByGame = 0;
static int32 GReloadsByHand = 0;

static FFortItemEntry* FindBotEntry(ABotController* Controller, FGuid Guid)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory)
        return nullptr;

    auto& Entries = Inventory->Inventory.ReplicatedEntries;
    for (int i = 0; i < Entries.Num(); i++)
    {
        auto& Entry = Entries.Get(i, FFortItemEntry::Size());
        if (Entry.ItemGuid == Guid)
            return &Entry;
    }
    return nullptr;
}

static void SyncBotEntry(ABotController* Controller, FFortItemEntry* Entry)
{
    auto Inventory = Controller ? Controller->Inventory : nullptr;
    if (!Inventory || !Entry)
        return;

    for (auto& Instance : Inventory->Inventory.ItemInstances)
        if (Instance && Instance->ItemEntry.ItemGuid == Entry->ItemGuid)
        {
            Instance->ItemEntry.LoadedAmmo = Entry->LoadedAmmo;
            if (FFortItemEntry::HasPhantomReserveAmmo())
                Instance->ItemEntry.PhantomReserveAmmo = Entry->PhantomReserveAmmo;
        }
    Inventory->Inventory.MarkItemDirty(*Entry);
}

static void TopUpBotReserve(ABotController* Controller, ABotWeapon* Weapon, int32 Clip)
{
    auto WeaponData = (const UFortItemDefinition*)Weapon->WeaponData;
    auto WeaponDefinition = WeaponData ? WeaponData->Cast<UFortWeaponItemDefinition>() : nullptr;
    if (!WeaponDefinition)
        return;

    if (FFortItemEntry::HasPhantomReserveAmmo() && WeaponDefinition->HasbUsesPhantomReserveAmmo() && WeaponDefinition->bUsesPhantomReserveAmmo)
    {
        auto Entry = FindBotEntry(Controller, Weapon->ItemEntryGuid);
        if (Entry && Entry->PhantomReserveAmmo < Clip)
        {
            Entry->PhantomReserveAmmo = Clip;
            SyncBotEntry(Controller, Entry);
        }
        return;
    }

    auto Ammo = ((UFortWorldItemDefinition*)WeaponDefinition)->GetAmmoWorldItemDefinition_BP();
    if (!Ammo || (const void*)Ammo == (const void*)WeaponDefinition)
        return;

    int32 Have = CountBotItem(Controller, Ammo);
    if (Have < Clip)
        AddBotItem(Controller, Ammo, Clip - Have, 0);
}

static bool HasReloadFunction(ABotWeapon* Weapon)
{
    static int Known = -1;
    if (Known == -1)
    {
        Known = Weapon->GetFunction("Reload") ? 1 : 0;
        BotLog("[RELOAD] Weapon reload function on this build: %s.\n", Known ? "found" : "MISSING, a bot's empty magazine gets filled by hand after 4s");
    }
    return Known == 1;
}

static bool GuardedReload(ABotWeapon* Weapon)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        Weapon->Reload();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;
    return bOk;
}

static bool TickReload(FBot& Bot, ABotController* Controller, ABotPawn* Pawn, bool bInFight, double Time)
{
    static auto RangedWeaponClass = AFortWeaponRanged::StaticClass();
    auto Weapon = (ABotWeapon*)Pawn->CurrentWeapon;
    if (!Weapon || !RangedWeaponClass || !Weapon->IsA(RangedWeaponClass) || !Weapon->HasAmmoCount() || !IsGun(Weapon->WeaponData))
    {
        Bot.ReloadStartTime = -1;
        return false;
    }

    auto Stats = GetGunStats(Weapon->WeaponData);
    int32 Clip = Stats ? Stats->ClipSize : 0;
    if (Clip <= 0)
    {
        Bot.ReloadStartTime = -1;
        return false;
    }

    int32 Ammo = Weapon->AmmoCount;

    if (Bot.ReloadStartTime >= 0)
    {
        double Waited = Time - Bot.ReloadStartTime;
        if (Ammo >= Clip)
        {
            Bot.ReloadStartTime = -1;
            if (++GReloadsByGame <= 3)
                BotLog("[RELOAD] A bot's %s reloaded by the game itself (%d/%d) after %.1fs.\n", Weapon->WeaponData->Name.ToString().c_str(), Ammo, Clip, Waited);
            return false;
        }

        if (bInFight && Ammo > 0)
        {
            Bot.ReloadStartTime = -1;
            return false;
        }

        if (Waited > 4.0)
        {
            if (auto Entry = FindBotEntry(Controller, Weapon->ItemEntryGuid))
            {
                Entry->LoadedAmmo = Clip;
                SyncBotEntry(Controller, Entry);
            }
            Weapon->AmmoCount = Clip;
            Bot.ReloadStartTime = -1;
            if (++GReloadsByHand <= 3)
                BotLog("[RELOAD] A bot's %s didn't reload in %.1fs (%d/%d), filled the magazine by hand.\n", Weapon->WeaponData->Name.ToString().c_str(), Waited, Ammo, Clip);
            return false;
        }

        return Ammo <= 0;
    }

    bool bEmpty = Ammo <= 0;
    bool bTopOff = !bInFight && Ammo < Clip && Time - Bot.LastTargetTime > 3.0;
    if (!bEmpty && !bTopOff)
        return false;

    TopUpBotReserve(Controller, Weapon, Clip);
    Bot.ReloadStartTime = Time;
    bool bAsked = GuardedReload(Weapon);
    if (++GReloadsAsked <= 3)
        BotLog("[RELOAD] A bot's %s is at %d/%d %s, asked the game to reload it%s.\n", Weapon->WeaponData->Name.ToString().c_str(), Ammo, Clip,
               bEmpty ? "(empty)" : "(topping off between fights)", !bAsked ? " (the call faulted, it'll be filled by hand)" : HasReloadFunction(Weapon) ? "" : " (no reload function on this build, it'll be filled by hand)");
    return bEmpty;
}

extern uint64_t CantBuild_;

struct FBotBuildPad
{
    uint8 Bytes[0x18];
};

static UClass* FindBuildClass(const char* ShortName)
{
    for (auto& [Handle, Class] : AFortGameStateAthena::BuildingClassMap)
    {
        auto Found = (UClass*)Class.ClassPtr;
        if (Found && strcmp(Found->Name.ToString().c_str(), ShortName) == 0)
            return Found;
    }
    return (UClass*)FindClass(ShortName);
}

static UClass* BotWallClass(int Material)
{
    static UClass* Walls[3] = {};
    static bool bResolved = false;
    if (!bResolved)
    {
        bResolved = true;
        Walls[0] = FindBuildClass("PBWA_W1_Solid_C");
        Walls[1] = FindBuildClass("PBWA_S1_Solid_C");
        Walls[2] = FindBuildClass("PBWA_M1_Solid_C");
        BotLog("[BUILD] Bot wall pieces: wood %s, brick %s, metal %s.\n", Walls[0] ? "found" : "MISSING", Walls[1] ? "found" : "MISSING", Walls[2] ? "found" : "MISSING");
    }
    return Walls[Material] ? Walls[Material] : Walls[0];
}

static UClass* BotRampClass(int Material)
{
    static UClass* Ramps[3] = {};
    static bool bResolved = false;
    if (!bResolved)
    {
        bResolved = true;
        Ramps[0] = FindBuildClass("PBWA_W1_StairW_C");
        Ramps[1] = FindBuildClass("PBWA_S1_StairW_C");
        Ramps[2] = FindBuildClass("PBWA_M1_StairW_C");
        BotLog("[BUILD] Bot ramp pieces: wood %s, brick %s, metal %s.\n", Ramps[0] ? "found" : "MISSING", Ramps[1] ? "found" : "MISSING", Ramps[2] ? "found" : "MISSING");
    }
    return Ramps[Material] ? Ramps[Material] : Ramps[0];
}

static bool bBotBuildingOff = false;
static int32 GBotPieces = 0;
static int32 GBotPiecesRefused = 0;
static int32 GBotBuildsThisTick = 0;

static ABuildingSMActor* SpawnBotPiece(ABotController* Controller, AFortPlayerStateAthena* PlayerState, UClass* Class, FVector Location, FRotator Rotation)
{
    if (!Class || !CantBuild_)
        return nullptr;

    TArray<ABuildingSMActor*> InTheWay;
    char Unknown = 0;
    TSubclassOf<AActor> ClassRef = (const UClass*)Class;
    auto CantBuild = (__int64 (*)(UWorld*, TSubclassOf<AActor>&, FBotBuildPad, FBotBuildPad, bool, TArray<ABuildingSMActor*>*, char*))CantBuild_;
    bool bBlocked = CantBuild(UWorld::GetWorld(), ClassRef, *(FBotBuildPad*)&Location, *(FBotBuildPad*)&Rotation, false, &InTheWay, &Unknown) != 0;
    int32 Replaced = InTheWay.Num();
    InTheWay.Free();
    if (bBlocked || Replaced > 0)
    {
        GBotPiecesRefused++;
        return nullptr;
    }

    auto Building = UWorld::SpawnActorUnfinished<ABuildingSMActor>(Class, Location, Rotation, Controller);
    if (!Building)
        return nullptr;

    Building->InitializeKismetSpawnedBuildingActor(Building, (AActor*)nullptr, true, (AActor*)nullptr, false);
    UWorld::FinishSpawnActor(Building, Location, Rotation);

    if (Building->HasCurrentBuildingLevel())
    {
        Building->CurrentBuildingLevel = 0;
        Building->OnRep_CurrentBuildingLevel();
    }
    Building->SetMirrored(false);
    Building->bPlayerPlaced = true;
    if (PlayerState && PlayerState->HasTeamIndex() && Building->HasTeam())
        Building->Team = PlayerState->TeamIndex;
    if (Building->HasTeamIndex() && Building->HasTeam())
        Building->TeamIndex = Building->Team;
    Building->ForceNetUpdate();

    GBotPieces++;
    GBotBuildsThisTick++;
    return Building;
}

static bool GuardedSpawnBotPiece(ABotController* Controller, AFortPlayerStateAthena* PlayerState, UClass* Class, const FVector& Location, const FRotator& Rotation)
{
    if (bBotBuildingOff)
        return false;

    bool bOk = true;
    bool bPlaced = false;
    GCrashGuardDepth++;
    __try
    {
        bPlaced = SpawnBotPiece(Controller, PlayerState, Class, Location, Rotation) != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
    {
        bBotBuildingOff = true;
        BotLog("[BUILD] Placing a bot's build piece faulted, bots stop building for this match.\n");
    }
    return bPlaced;
}

static int SideToward(const FVector& From, const FVector& To)
{
    double Angle = atan2(To.Y - From.Y, To.X - From.X) * 57.29577951308232;
    return (((int)lround(Angle / 90.0)) % 4 + 4) % 4;
}

static double SideYaw(int Side)
{
    static const double Yaws[] = { 0.0, 90.0, 180.0, -90.0 };
    return Yaws[Side & 3];
}

static void WallSpot(const FVector& Feet, int Side, double Level, FVector& OutLocation, FRotator& OutRotation)
{
    double TileX = floor(Feet.X / 512.0), TileY = floor(Feet.Y / 512.0);
    switch (Side & 3)
    {
    case 0:
        OutLocation = FVector((TileX + 1.0) * 512.0, (TileY + 0.5) * 512.0, Level);
        break;
    case 1:
        OutLocation = FVector((TileX + 0.5) * 512.0, (TileY + 1.0) * 512.0, Level);
        break;
    case 2:
        OutLocation = FVector(TileX * 512.0, (TileY + 0.5) * 512.0, Level);
        break;
    default:
        OutLocation = FVector((TileX + 0.5) * 512.0, TileY * 512.0, Level);
        break;
    }
    OutRotation = MakeRotator(0, SideYaw(Side));
}

static int PlaceBotWallAt(ABotController* Controller, AFortPlayerStateAthena* PlayerState, const FVector& Feet, int Side, int Material, double Level)
{
    FVector Location;
    FRotator Rotation;
    WallSpot(Feet, Side, Level, Location, Rotation);
    return GuardedSpawnBotPiece(Controller, PlayerState, BotWallClass(Material), Location, Rotation) ? 1 : 0;
}

static int PlaceBotWall(ABotController* Controller, AFortPlayerStateAthena* PlayerState, const FVector& Feet, int Side, int Material)
{
    double Level = floor(Feet.Z / 384.0) * 384.0;
    double Cover = (std::min)(Level + 384.0 - Feet.Z, 180.0);

    int Placed = PlaceBotWallAt(Controller, PlayerState, Feet, Side, Material, Level);
    if (Cover < 120.0)
        Placed += PlaceBotWallAt(Controller, PlayerState, Feet, Side, Material, Level + 384.0);
    return Placed;
}

static double BotRampLevel(const FVector& Feet)
{
    return floor((Feet.Z + 40.0) / 384.0) * 384.0;
}

static int PlaceBotRamp(ABotController* Controller, AFortPlayerStateAthena* PlayerState, const FVector& Feet, int Side, int Material)
{
    double Level = BotRampLevel(Feet);
    double TileX = floor(Feet.X / 512.0), TileY = floor(Feet.Y / 512.0);
    FRotator Rotation = MakeRotator(0, SideYaw(Side + 1));

    FVector Spots[2];
    if ((Side & 1) == 0)
    {
        Spots[0] = FVector((TileX + 0.5) * 512.0, TileY * 512.0, Level);
        Spots[1] = FVector((TileX + 0.5) * 512.0, (TileY + 1.0) * 512.0, Level);
    }
    else
    {
        Spots[0] = FVector(TileX * 512.0, (TileY + 0.5) * 512.0, Level);
        Spots[1] = FVector((TileX + 1.0) * 512.0, (TileY + 0.5) * 512.0, Level);
    }

    int Placed = 0;
    for (auto& Spot : Spots)
        Placed += GuardedSpawnBotPiece(Controller, PlayerState, BotRampClass(Material), Spot, Rotation) ? 1 : 0;
    return Placed;
}

static void TickBuilding(FBot& Bot, ABotController* Controller, ABotPawn* Pawn, ABotPawn* Threat, double Time)
{
    if (bBotBuildingOff || !Threat || Time < Bot.NextBuildTime || GBotBuildsThisTick >= 4)
        return;
    if (Time - Bot.LastHurtTime > 1.5)
        return;
    if (Pawn->HasbIsInAnyStorm() && Pawn->bIsInAnyStorm)
        return;

    auto Movement = (UBotMovement*)Pawn->CharacterMovement;
    if (!Movement || !Movement->IsMovingOnGround())
        return;

    if (Time - Bot.BuildWindowStart > 12.0)
    {
        Bot.BuildWindowStart = Time;
        Bot.BuildsThisFight = 0;
    }
    if (Bot.BuildsThisFight >= 10)
        return;

    auto Location = Pawn->K2_GetActorLocation();
    auto ThreatLocation = Threat->K2_GetActorLocation();
    if (Dist2D(Location, ThreatLocation) > 6000.0)
        return;

    auto PlayerState = GetWeak<AFortPlayerStateAthena>(Bot.PlayerState);
    FVector Feet = Location;
    Feet.Z -= 90.0;
    int Side = SideToward(Location, ThreatLocation);
    int Material = RandInt(0, 9) < 7 ? 0 : (RandInt(0, 2) < 2 ? 1 : 2);
    float Health = Pawn->GetHealth() + Pawn->GetShield();

    const char* What = "a wall";
    int Placed = 0;
    if (Health < 45.f)
    {
        What = "a box";
        for (int Each = 0; Each < 4; Each++)
            Placed += PlaceBotWall(Controller, PlayerState, Feet, Each, Material);
    }
    else if (ThreatLocation.Z - Location.Z > 250.0 || RandInt(0, 99) < 30)
    {
        What = "a ramp toward its attacker";
        Placed += PlaceBotRamp(Controller, PlayerState, Feet, Side, Material);
        if (Placed)
            Placed += PlaceBotWallAt(Controller, PlayerState, Feet, Side, Material, BotRampLevel(Feet) + 384.0);
    }
    else
        Placed += PlaceBotWall(Controller, PlayerState, Feet, Side, Material);

    Bot.NextBuildTime = Time + (Placed ? RandRange(0.9f, 1.8f) : 0.6f);
    Bot.BuildsThisFight += Placed;

    static int Reported = 0;
    if (Placed && ++Reported <= 12)
        BotLog("[BUILD] A bot that was being shot built %s (%d piece(s)). %d bot pieces so far, %d spots refused by the game's build check.\n", What, Placed, GBotPieces,
               GBotPiecesRefused);
}

static bool TickCombat(AFortGameMode* GameMode, FBot& Bot, ABotController* Controller, ABotPawn* Pawn, double Time)
{
    if ((EAthenaGamePhase)GetGamePhase() < EAthenaGamePhase::SafeZones)
    {
        StopFiring(Bot, Pawn);
        Bot.Target = FWeakObjectPtr();
        return false;
    }

    auto Attacker = RecentAttacker(Bot, Pawn, Time);
    if (Time >= Bot.NextTargetSearchTime)
    {
        Bot.NextTargetSearchTime = Time + 0.5;
        double Range = IsHoldingGun(Pawn) ? 7000.0 : 2500.0;
        if (Attacker)
            Range = (std::max)(Range, 7000.0);
        Bot.Target = FWeakObjectPtr(FindEnemy(GameMode, Bot, Controller, Pawn, Range, Attacker));
    }

    auto Target = GetWeak<ABotPawn>(Bot.Target);
    if (!Target || IsPawnDead(Target))
    {
        Bot.Target = FWeakObjectPtr();
        StopFiring(Bot, Pawn);
        TickReload(Bot, Controller, Pawn, false, Time);
        TickBuilding(Bot, Controller, Pawn, Attacker, Time);
        return false;
    }

    Bot.LastTargetTime = Time;
    auto Location = Pawn->K2_GetActorLocation();
    auto TargetLocation = Target->K2_GetActorLocation();
    auto ToTarget = TargetLocation - Location;
    double Distance = ToTarget.Magnitude();
    bool bGun = IsHoldingGun(Pawn);

    if (Time >= Bot.NextAimErrorTime)
    {
        float Error = (1.f - FConfiguration::BotAccuracy) * 6.f;
        Bot.AimErrorPitch = RandRange(-Error, Error) * 0.5f;
        Bot.AimErrorYaw = RandRange(-Error, Error);
        Bot.NextAimErrorTime = Time + 0.3;
    }
    auto Aim = LookAt(Location + FVector(0, 0, 60), TargetLocation + FVector(0, 0, 40));
    Aim.Pitch = Aim.Pitch + Bot.AimErrorPitch;
    Aim.Yaw = Aim.Yaw + Bot.AimErrorYaw;
    Controller->SetControlRotation(Aim);

    TickBuilding(Bot, Controller, Pawn, Attacker ? Attacker : Target, Time);

    bool bReloading = bGun && TickReload(Bot, Controller, Pawn, true, Time);
    bool bInRange = bGun ? Distance < 6000.0 : Distance < 250.0;
    if (bReloading)
        StopFiring(Bot, Pawn);
    else if (bInRange && Time >= Bot.NextFireToggleTime)
    {
        if (Bot.bFiring)
        {
            StopFiring(Bot, Pawn);
            Bot.NextFireToggleTime = Time + RandRange(0.15f, 0.5f);
        }
        else if (!Bot.bTriggerFaulted)
        {
            if (GuardedStartFire(Pawn))
                Bot.bFiring = true;
            else
                Bot.bTriggerFaulted = true;
            Bot.NextFireToggleTime = Time + RandRange(0.4f, 1.2f);
        }
    }
    else if (!bInRange)
        StopFiring(Bot, Pawn);

    if (bGun && bInRange && !bReloading)
        TickBotShots(Bot, Controller, Pawn, Target, Distance, Time);

    if (Bot.bRotating)
        return true;

    double Ideal = bGun ? 1500.0 : 120.0;
    auto Forward = FVector(ToTarget.X, ToTarget.Y, 0);
    if (Forward.Magnitude() > 1.0)
        Forward = Forward / Forward.Magnitude();

    if (Distance > Ideal + 300.0)
        SetGoal(Bot, TargetLocation, Time);
    else
    {
        if (Time >= Bot.StrafeUntil)
        {
            Bot.StrafeUntil = Time + RandRange(0.6f, 1.6f);
            Bot.StrafeDirection = RandRange(0.f, 1.f) < 0.5f ? -1.f : 1.f;
            if (RandInt(0, 6) == 0)
                Pawn->Jump();
        }
        auto Strafe = FVector(-Forward.Y, Forward.X, 0) * Bot.StrafeDirection;
        if (Distance < Ideal - 300.0)
            Strafe = Strafe - Forward;
        SetGoal(Bot, Location + Strafe * 400.0, Time);
    }

    return true;
}

static void (*TearOffOG)(AActor*) = nullptr;
static bool bTearOffDeferralOn = false;

struct FDeferredTearOff
{
    FWeakObjectPtr Pawn;
    double Time;
};

static std::vector<FDeferredTearOff> DeferredTearOffs;
static int32 GTearOffsDeferred = 0;

static bool IsBotPawn(const AActor* Actor)
{
    for (auto& Bot : AllBots)
        if (Bot.Pawn.Get() == (const UObject*)Actor)
            return true;
    return false;
}

static void TearOffHook(AActor* Actor)
{
    if (bTearOffDeferralOn && Actor && IsBotPawn(Actor))
    {
        for (auto& Deferred : DeferredTearOffs)
            if (Deferred.Pawn.Get() == (const UObject*)Actor)
                return;

        DeferredTearOffs.push_back({ FWeakObjectPtr(Actor), Now() });
        if (++GTearOffsDeferred <= 3)
            BotLog("[DMGNUM] A bot was eliminated, its tear-off waits a moment so the killing hit's cues (damage number, hit effect) still reach clients.\n");
        return;
    }

    TearOffOG(Actor);
}

static void FlushDeferredTearOffs(double Time)
{
    for (size_t i = 0; i < DeferredTearOffs.size();)
    {
        if (Time >= DeferredTearOffs[i].Time && Time - DeferredTearOffs[i].Time < 0.1)
        {
            i++;
            continue;
        }

        auto Pawn = (AActor*)DeferredTearOffs[i].Pawn.Get();
        DeferredTearOffs.erase(DeferredTearOffs.begin() + i);
        if (Pawn && !Pawn->bActorIsBeingDestroyed && TearOffOG)
            TearOffOG(Pawn);
    }
}

static bool WritesTearOffFlag(uint8* Code, uint32 FlagOffset, uint8 FlagMask)
{
    auto Ip = Code;
    for (int Count = 0; Count < 64 && Ip - Code < 0x180; Count++)
    {
        hde64s Insn;
        hde64_disasm(Ip, &Insn);
        if (Insn.flags & F_ERROR)
            return false;

        if (Insn.opcode == 0x80 && Insn.modrm_reg == 1 && (Insn.modrm_mod == 1 || Insn.modrm_mod == 2))
        {
            uint32 Disp = Insn.modrm_mod == 1 ? (uint32)(int32)(int8)Insn.disp.disp8 : Insn.disp.disp32;
            if (Disp == FlagOffset && (Insn.imm.imm8 & FlagMask) == FlagMask)
                return true;
        }

        if (Insn.opcode == 0xCC)
            return false;
        Ip += Insn.len;
    }
    return false;
}

static bool bTearOffHookTried = false;

static void HookBotTearOff(ABotPawn* Pawn)
{
    if (bTearOffHookTried || !Pawn)
        return;
    bTearOffHookTried = true;

    auto FlagProp = Pawn->GetProperty("bTearOff", 0x20000);
    auto Function = Pawn->GetFunction("TearOff");
    auto Code = Function ? (uint8*)Function->GetNativeFunc() : nullptr;
    if (!FlagProp || !Code)
    {
        BotLog("[DMGNUM] This build has no %s, a bot's killing hit can lose its damage number.\n", FlagProp ? "TearOff function" : "bTearOff flag");
        return;
    }

    uint32 FlagOffset = GetFromOffset<uint32>(FlagProp, Offsets::Offset_Internal);
    uint8 FlagMask = FlagProp->GetFieldMask();
    if (!FlagMask || FlagMask == 0xFF)
        FlagMask = 1;

    auto Text = Memcury::PE::Section::GetSection(".text");
    auto VTable = (void**)Pawn->Vft;
    int32 Index = -1;
    uint8* Direct = nullptr;
    auto Ip = Code;
    for (int Count = 0; Count < 24 && Ip - Code < 0x60; Count++)
    {
        hde64s Insn;
        hde64_disasm(Ip, &Insn);
        if (Insn.flags & F_ERROR)
            break;

        if (Insn.opcode == 0xFF && (Insn.modrm_reg == 2 || Insn.modrm_reg == 4) && (Insn.modrm_mod == 1 || Insn.modrm_mod == 2))
        {
            uint32 Disp = Insn.modrm_mod == 1 ? (uint32)Insn.disp.disp8 : Insn.disp.disp32;
            if (Disp % 8 == 0 && Disp / 8 < 0x500)
                Index = (int32)(Disp / 8);
            break;
        }
        if (Insn.opcode == 0xE8 || Insn.opcode == 0xE9)
        {
            Direct = Ip + Insn.len + (int32)Insn.imm.imm32;
            break;
        }
        if (Insn.opcode == 0xC3 || Insn.opcode == 0xCC)
            break;
        Ip += Insn.len;
    }

    if (Index >= 0 && Text.isInSection(Memcury::PE::Address((uintptr_t)VTable[Index])) && WritesTearOffFlag((uint8*)VTable[Index], FlagOffset, FlagMask))
    {
        TearOffOG = (void (*)(AActor*))VTable[Index];
        DWORD Old;
        VirtualProtect(&VTable[Index], sizeof(void*), PAGE_EXECUTE_READWRITE, &Old);
        VTable[Index] = (void*)TearOffHook;
        VirtualProtect(&VTable[Index], sizeof(void*), Old, &Old);
        bTearOffDeferralOn = true;
        BotLog("[DMGNUM] Bot tear-offs are delayed a moment (pawn vtable slot %d), so killing hits keep their damage numbers.\n", Index);
        return;
    }

    if (Direct && Text.isInSection(Memcury::PE::Address((uintptr_t)Direct)) && WritesTearOffFlag(Direct, FlagOffset, FlagMask))
    {
        Hooking::Hook(Direct, (void*)TearOffHook, TearOffOG);
        bTearOffDeferralOn = TearOffOG != nullptr;
        BotLog("[DMGNUM] Bot tear-offs are delayed a moment (function hook %s), so killing hits keep their damage numbers.\n", bTearOffDeferralOn ? "in" : "FAILED");
        return;
    }

    BotLog("[DMGNUM] Couldn't confirm where this build's TearOff is (slot %d, direct %s), left it alone: a bot's killing hit can still lose its damage number.\n", Index,
           Direct ? "found" : "none");
}

static void GuardedHookBotTearOff(ABotPawn* Pawn)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        HookBotTearOff(Pawn);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
    {
        bTearOffHookTried = true;
        bTearOffDeferralOn = false;
        BotLog("[DMGNUM] Looking for this build's TearOff faulted, left it alone: a bot's killing hit can still lose its damage number.\n");
    }
}

static int32 PushedBotsThisTick = 0;

static void TickSteering(FBot& Bot, ABotController* Controller, ABotPawn* Pawn, double Time, bool bFaceGoal)
{
    double Delta = Bot.LastMoveTime > 0 ? std::clamp(Time - Bot.LastMoveTime, 0.0, 0.1) : 0.0;
    Bot.LastMoveTime = Time;

    if (!Bot.bHasMoveGoal)
        return;

    auto Location = Pawn->K2_GetActorLocation();
    auto ToGoal = Bot.MoveGoal - Location;
    ToGoal.Z = 0;
    double Distance = ToGoal.Magnitude();
    if (Distance < 150.0)
    {
        Bot.bHasMoveGoal = false;
        return;
    }

    auto Direction = ToGoal / Distance;
    auto Movement = (UBotMovement*)Pawn->CharacterMovement;

    if (Controller->IsMoveInputIgnored())
        Controller->ResetIgnoreMoveInput();
    if (Movement && ReadByteProp(Movement, "MovementMode") == 0 )
        Movement->SetMovementMode((uint8)1 , (uint8)0);

    Pawn->AddMovementInput(Direction, 1.f, true);
    if (bFaceGoal)
        Controller->SetControlRotation(MakeRotator(0, atan2(Direction.Y, Direction.X) * 57.29577951308232));

    if (Movement)
    {
        auto Velocity = Movement->Velocity;
        Velocity.Z = 0;
        if (Velocity.Magnitude() < 150.0)
            Bot.SlowTime += Delta;
        else
            Bot.SlowTime = 0;

        if (Bot.SlowTime > 0.5 && Delta > 0.0)
            PushedBotsThisTick++;
    }

    if (Time >= Bot.NextStuckCheckTime)
    {
        if (Bot.NextStuckCheckTime > 0 && Dist2D(Location, Bot.LastStuckLocation) < 60.0)
        {
            Bot.StuckCount++;
            Pawn->Jump();
            if (Bot.StuckCount >= 3)
            {
                float Side = RandRange(0.f, 1.f) < 0.5f ? -1.f : 1.f;
                auto Detour = Location + FVector(-Direction.Y, Direction.X, 0) * (Side * RandRange(1200.f, 2500.f)) + Direction * 400.0;
                SetGoal(Bot, Detour, Time);
                Bot.DetourUntil = Time + 3.0;
                Bot.LootTarget = FWeakObjectPtr();
                Bot.StuckCount = 0;
            }
        }
        else
            Bot.StuckCount = 0;

        Bot.LastStuckLocation = Location;
        Bot.NextStuckCheckTime = Time + 1.0;
    }
}

static void TickBrain(AFortGameMode* GameMode, FBot& Bot, ABotController* Controller, ABotPawn* Pawn, uint8 GamePhase, double Time)
{
    if ((EAthenaGamePhase)GamePhase <= EAthenaGamePhase::Warmup)
    {
        StopFiring(Bot, Pawn);
        if (auto Movement = Pawn->CharacterMovement)
        {
            Movement->Velocity = FVector();
        }
        return;
    }

    if (Time >= Bot.NextNativeBrainCheck)
    {
        auto Brain = (UBotBrain*)Controller->BrainComponent;
        Bot.bNativeBrainRunning = Brain && Brain->IsRunning();
        Bot.NextNativeBrainCheck = Time + 2.0;
    }
    if (Bot.bNativeBrainRunning)
        return;

    TickDirector(GameMode, Bot, Controller, Pawn, GamePhase, Time);
    TryGrabLoot(Bot, Controller, Pawn);

    bool bFighting = TickCombat(GameMode, Bot, Controller, Pawn, Time);
    TickSteering(Bot, Controller, Pawn, Time, !bFighting);
}

static void GuardedBrain(AFortGameMode* GameMode, FBot* Bot, ABotController* Controller, ABotPawn* Pawn, uint8 GamePhase, double Time)
{
    static int Faults = 0;
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        TickBrain(GameMode, *Bot, Controller, Pawn, GamePhase, Time);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk && Faults++ < 5)
        BotLog("[Bots] Bot brain faulted (%d), skipped that tick.\n", Faults);
}

static void LogBotStatus(uint8 GamePhase, double Time)
{
    static double NextStatusTime = 0;
    if (Time < NextStatusTime)
        return;
    NextStatusTime = Time + 15.0;

    int Playing = 0, OnBus = 0, Skydiving = 0, BrainsRunning = 0, Moving = 0, Armed = 0, Rotating = 0;
    for (auto& Bot : AllBots)
    {
        auto Controller = GetWeak<ABotController>(Bot.Controller);
        auto Pawn = GetWeak<ABotPawn>(Bot.Pawn);
        if (Bot.State == EBotState::Dead || !Controller || !Pawn)
            continue;

        Playing += Bot.State == EBotState::Playing;
        OnBus += Bot.State == EBotState::OnBus;
        Skydiving += Bot.State == EBotState::Skydiving;
        Rotating += Bot.bRotating;

        auto Brain = (UBotBrain*)Controller->BrainComponent;
        BrainsRunning += Brain && Brain->IsRunning();
        Armed += HasRangedWeapon(Controller);

        if (auto Movement = Pawn->CharacterMovement)
        {
            auto Velocity = Movement->Velocity;
            Velocity.Z = 0;
            Moving += Velocity.Magnitude() > 50.0;
        }
    }


    BotLog("[Bots] Status (phase %d): %d playing, %d on bus, %d skydiving | brains running %d, moving %d, armed %d, rotating %d, stuck %d\n", GamePhase, Playing, OnBus,
           Skydiving, BrainsRunning, Moving, Armed, Rotating, PushedBotsThisTick);
    if ((EAthenaGamePhase)GamePhase >= EAthenaGamePhase::SafeZones)
        BotLog("[Bots] Since the bus: reloads asked %d (finished by the game %d, filled by hand %d), build pieces %d (%d spots refused), pickups left on the ground %d, "
               "swaps at 5 slots %d, delayed tear-offs %d\n",
               GReloadsAsked, GReloadsByGame, GReloadsByHand, GBotPieces, GBotPiecesRefused, GPickupsDeclined, GPickupsSwapped, GTearOffsDeferred);
    PushedBotsThisTick = 0;
}

static const UClass* FindImpulseAttributeSet()
{
    static const UClass* SetClass = nullptr;
    static bool bSearched = false;
    if (bSearched)
        return SetClass;
    bSearched = true;

    auto AttributeSetClass = FindClass("AttributeSet");
    for (int i = 0; AttributeSetClass && i < TUObjectArray::Num(); i++)
    {
        auto Object = TUObjectArray::GetObjectByIndex(i);
        if (!Object || !Object->IsDefaultObject() || !Object->IsA(AttributeSetClass))
            continue;
        if (Object->GetOffset("ImpulseMagnitude") != -1)
        {
            SetClass = Object->Class;
            break;
        }
    }

    BotLog("[KNOCKBACK] Knockback attribute set on this build: %s. Players and bots don't get it, so shotgun hits don't push anyone.\n",
           SetClass ? SetClass->Name.ToString().c_str() : "not found");
    return SetClass;
}

static int32 RemoveKnockbackAttributes(AFortPlayerStateAthena* PlayerState)
{
    auto SetClass = FindImpulseAttributeSet();
    auto AbilitySystem = PlayerState ? (UObject*)PlayerState->AbilitySystemComponent : nullptr;
    if (!SetClass || !AbilitySystem)
        return 0;

    auto SpawnedOffset = AbilitySystem->GetOffset("SpawnedAttributes");
    if (SpawnedOffset == -1)
        return 0;

    auto& Spawned = *(TArray<UObject*>*)(__int64(AbilitySystem) + SpawnedOffset);
    int32 Removed = 0;
    for (int i = Spawned.Num() - 1; i >= 0; i--)
        if (Spawned[i] && Spawned[i]->IsA(SetClass))
        {
            Spawned.Remove(i);
            Removed++;
        }

    if (Removed)
        if (auto OnRep = AbilitySystem->GetFunction("OnRep_SpawnedAttributes"))
        {
            struct
            {
                TArray<UObject*> PreviousSpawnedAttributes;
            } Params{};
            AbilitySystem->ProcessEvent(OnRep, &Params);
        }
    return Removed;
}

static void TickKnockbackAttributes(AFortGameMode* GameMode, double Time)
{
    static double NextCheckTime = 0;
    if (Time < NextCheckTime)
        return;
    NextCheckTime = Time + 2.0;

    static int32 PlayersChecked = 0;
    int32 FromPlayers = 0, FromBots = 0;
    for (auto& Player : GameMode->AlivePlayers)
        if (auto PlayerController = (AFortPlayerControllerAthena*)Player)
            if (PlayerController->PlayerState)
            {
                FromPlayers += RemoveKnockbackAttributes(PlayerController->PlayerState);
                PlayersChecked++;
            }

    for (auto& Bot : AllBots)
        if (Bot.State != EBotState::Dead)
            FromBots += RemoveKnockbackAttributes(GetWeak<AFortPlayerStateAthena>(Bot.PlayerState));

    static int32 Reports = 0;
    if ((FromPlayers || FromBots) && ++Reports <= 5)
        BotLog("[KNOCKBACK] Something gave %d player(s) and %d bot(s) the knockback attribute set, took it off again (no shotgun knockback).\n", FromPlayers, FromBots);

    static bool bConfirmed = false;
    if (!bConfirmed && PlayersChecked > 0 && !FromPlayers)
    {
        bConfirmed = true;
        BotLog("[KNOCKBACK] The player's ability system has no knockback attribute set, shotgun hits can't push them.\n");
    }
}

static void GuardedKnockbackAttributes(AFortGameMode* GameMode, double Time)
{
    static bool bLogged = false;
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        TickKnockbackAttributes(GameMode, Time);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk && !bLogged)
    {
        bLogged = true;
        BotLog("[KNOCKBACK] Checking for the knockback attribute set faulted.\n");
    }
}


static void SafeForceKill(ABotPawn* Pawn)
{
    if (!Pawn)
        return;
    GCrashGuardDepth++;
    __try
    {
        Pawn->ForceKill(FGameplayTag(), (AActor*)nullptr, (AActor*)nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    GCrashGuardDepth--;
}

static bool SetBoolProp(UObject* Object, const char* Name, bool bValue)
{
    auto Prop = Object ? Object->GetProperty(Name, 0x20000) : nullptr;
    if (!Prop)
        return false;

    auto Byte = (uint8*)Object + GetFromOffset<uint32>(Prop, Offsets::Offset_Internal);
    auto Mask = Prop->GetFieldMask();
    if (!Mask || Mask == 0xFF)
        *Byte = bValue ? 1 : 0;
    else if (bValue)
        *Byte |= Mask;
    else
        *Byte &= ~Mask;
    return true;
}

static void MakeBotDamageable(ABotPawn* Pawn, AFortPlayerStateAthena* PlayerState)
{
    bool bCouldBeDamaged = !Pawn->HasbCanBeDamaged() || Pawn->bCanBeDamaged;
    bool bWasInvulnerable = Pawn->HasbIsInvulnerable() && Pawn->bIsInvulnerable;

    if (!bCouldBeDamaged)
        Pawn->bCanBeDamaged = true;
    if (bWasInvulnerable)
    {
        Pawn->bIsInvulnerable = false;
        if (auto OnRep = Pawn->GetFunction("OnRep_bIsInvulnerable"))
            Pawn->ProcessEvent(OnRep, nullptr);
    }

    SetBoolProp(Pawn, "bShouldPawnDeathDisplayOnKillFeed", true);
    SetBoolProp(Pawn, "bShouldPawnDBNODisplayOnKillFeed", true);

    static int Reported = 0;
    if (++Reported <= 5)
        BotLog("[Bots] Bot landed: could be damaged %s, was invulnerable %s, health %.0f, shield %.0f, team %d.\n", bCouldBeDamaged ? "yes" : "NO (fixed)",
               bWasInvulnerable ? "YES (fixed)" : "no", Pawn->GetHealth(), Pawn->GetShield(), PlayerState ? (int)PlayerState->TeamIndex : -1);
}

static void TrackBotDamage(FBot& Bot, ABotPawn* Pawn, double Time)
{
    if (Time < Bot.NextDamageCheck)
        return;
    Bot.NextDamageCheck = Time + 0.5;

    float Health = Pawn->GetHealth();
    float Shield = Pawn->GetShield();
    if (Bot.LastHealth >= 0.f && (Health < Bot.LastHealth - 0.5f || Shield < Bot.LastShield - 0.5f))
    {
        Bot.LastHurtTime = Time;
        static int Reported = 0;
        if (++Reported <= 20)
            BotLog("[DMG] A bot took %.0f damage (health %.0f -> %.0f, shield %.0f -> %.0f).\n", (Bot.LastHealth - Health) + (Bot.LastShield - Shield), Bot.LastHealth,
                   Health, Bot.LastShield, Shield);
    }
    Bot.LastHealth = Health;
    Bot.LastShield = Shield;
}

static void KeepBotFallDamageOff(ABotPawn* Pawn)
{
    if (Pawn->HasbIgnoreNextFallingDamage())
        Pawn->bIgnoreNextFallingDamage = true;
    if (Pawn->HasLastFallDistance())
        Pawn->LastFallDistance = 0.f;
}

struct FFallWatch
{
    AActor* Pawn = nullptr;
    bool bAirborne = false;
    bool bGlided = false;
    double PeakZ = 0.0;
    double Drop = 0.0;
    float HealthBefore = 0.f;
    double CheckAt = -1.0;
};

static std::unordered_map<void*, FFallWatch> FallWatches;

static void WatchPlayerFalls(AFortGameMode* GameMode, double Time)
{
    static int Reports = 0;
    if (Reports >= 40)
        return;

    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    if (!GameState->HasPlayerArray())
        return;

    for (int i = 0; i < GameState->PlayerArray.Num(); i++)
    {
        auto PlayerState = GameState->PlayerArray[i];
        auto Owner = PlayerState ? (AActor*)PlayerState->Owner : nullptr;
        if (!Owner || !Owner->IsA<AFortPlayerControllerAthena>())
            continue;

        auto& Watch = FallWatches[(void*)Owner];
        auto Pawn = ((AFortPlayerControllerAthena*)Owner)->MyFortPawn;
        bool bPawnAlive = Pawn && !Pawn->bActorIsBeingDestroyed && Pawn->GetHealth() > 0.f;

        if (Watch.CheckAt >= 0 && (Time >= Watch.CheckAt || !bPawnAlive || (AActor*)Pawn != Watch.Pawn))
        {
            Watch.CheckAt = -1.0;
            Reports++;
            if (!bPawnAlive || (AActor*)Pawn != Watch.Pawn)
                BotLog("[FALL] A player landed from a %.0fm drop with %.0f health and died, the fall killed them.\n", Watch.Drop / 100.0, Watch.HealthBefore);
            else
            {
                float Health = Pawn->GetHealth();
                BotLog("[FALL] A player landed from a %.0fm drop: health %.0f -> %.0f (%s).\n", Watch.Drop / 100.0, Watch.HealthBefore, Health,
                       Health < Watch.HealthBefore - 0.5f ? "fall damage applied" : "no fall damage taken");
            }
        }

        if (!bPawnAlive || !Pawn->CharacterMovement)
        {
            Watch.bAirborne = false;
            continue;
        }

        if ((AActor*)Pawn != Watch.Pawn)
        {
            Watch = FFallWatch{};
            Watch.Pawn = (AActor*)Pawn;
        }

        auto Movement = Pawn->CharacterMovement;
        bool bFalling = Movement->HasMovementMode() && Movement->MovementMode == 3 ;
        bool bGliding = (Pawn->HasbIsSkydiving() && Pawn->bIsSkydiving) || (Pawn->HasbIsParachuteOpen() && Pawn->bIsParachuteOpen);
        double Z = Pawn->K2_GetActorLocation().Z;

        if (bFalling)
        {
            if (!Watch.bAirborne)
            {
                Watch.bAirborne = true;
                Watch.bGlided = false;
                Watch.PeakZ = Z;
            }
            Watch.PeakZ = (std::max)(Watch.PeakZ, Z);
            Watch.bGlided = Watch.bGlided || bGliding;
            Watch.HealthBefore = Pawn->GetHealth();
        }
        else if (Watch.bAirborne)
        {
            Watch.bAirborne = false;
            Watch.Drop = Watch.PeakZ - Z;
            if (!Watch.bGlided && Watch.Drop >= 800.0 && Watch.CheckAt < 0)
                Watch.CheckAt = Time + 0.5;
        }
    }

    if (FallWatches.size() > 256)
        FallWatches.clear();
}

static void GuardedWatchPlayerFalls(AFortGameMode* GameMode, double Time)
{
    static bool bFaulted = false;
    if (bFaulted)
        return;

    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        WatchPlayerFalls(GameMode, Time);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
    {
        bFaulted = true;
        BotLog("[FALL] Watching player landings faulted, the [FALL] log is off for the rest of this match.\n");
    }
}

namespace BotAI
{
    void ClearBotAnonymity();
}

static void GuardedClearBotAnonymity()
{
    static bool bFaulted = false;
    if (bFaulted)
        return;

    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        BotAI::ClearBotAnonymity();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;

    if (!bOk)
    {
        bFaulted = true;
        BotLog("[ANON] Turning off the bots' anonymous mode faulted, it's off for the rest of this match (names may show as Anonymous).\n");
    }
}

void Bots::Tick()
{
    if (!FConfiguration::bBotLobby)
        return;

    auto World = UWorld::GetWorld();
    auto GameMode = World ? (AFortGameMode*)World->AuthorityGameMode : nullptr;
    if (!GameMode || !GameMode->GameState || !GameMode->HasWarmupRequiredPlayerCount())
        return;

    auto Time = Now();
    auto GamePhase = GetGamePhase();
    auto GamePhaseStep = GetGamePhaseStep();

    if (GamePhase != LastGamePhase && (EAthenaGamePhase)GamePhase == EAthenaGamePhase::Aircraft)
        PutBotsOnBus();

    if (GamePhase != LastGamePhase || GamePhaseStep != LastGamePhaseStep)
    {
        for (auto& Bot : AllBots)
            if (auto Controller = GetWeak<ABotController>(Bot.Controller))
                if (Bot.State != EBotState::Dead)
                    UpdatePhaseKeys(Controller);

        LastGamePhase = GamePhase;
        LastGamePhaseStep = GamePhaseStep;
    }

    if ((EAthenaGamePhase)GamePhase == EAthenaGamePhase::Warmup)
    {
        auto GameState = (AFortGameStateAthena*)GameMode->GameState;
        if (GameState)
        {
            float NowF = (float)Time;
            float DesiredEnd = NowF + 25.f;
            if (GameState->HasWarmupCountdownEndTime())
            {
                float CurEnd = GameState->WarmupCountdownEndTime;
                if (CurEnd > DesiredEnd + 5.f || CurEnd < NowF)
                    GameState->WarmupCountdownEndTime = DesiredEnd;
            }
            if (GameMode->HasWarmupCountdownDuration())
                GameMode->WarmupCountdownDuration = 25.f;
            if (GameMode->HasWarmupRequiredPlayerCount())
                GameMode->WarmupRequiredPlayerCount = 1;

            static double FullSince = -1.0;
            int Total = (GameMode->HasAlivePlayers() ? GameMode->AlivePlayers.Num() : 0) + Bots::GetSpawnedBotCount();
            if (Total >= FConfiguration::AutoFillTargetPlayerCount)
            {
                if (FullSince < 0.0)
                    FullSince = Time;
                if (Time - FullSince >= 8.0)
                {
                    static bool Launched = false;
                    if (!Launched)
                    {
                        Launched = true;
                        BotLog("[Bots] Lobby full (%d) — starting aircraft.\n", Total);
                        UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"startaircraft"), nullptr);
                    }
                }
            }
            else
                FullSince = -1.0;
        }
    }

    SpawnPendingBots(GameMode, GamePhase);
    PickUpNewBots(GameMode, Time);

    GBotBuildsThisTick = 0;
    FlushDeferredTearOffs(Time);
    if (!bTearOffHookTried)
        for (auto& Bot : AllBots)
            if (auto Pawn = GetWeak<ABotPawn>(Bot.Pawn))
            {
                GuardedHookBotTearOff(Pawn);
                break;
            }

    static double NextAnonymityCheck = 0.0;
    if (SpawnedBots > 0 && Time >= NextAnonymityCheck)
    {
        NextAnonymityCheck = Time + 0.5;
        GuardedClearBotAnonymity();
    }
    if ((EAthenaGamePhase)GamePhase >= EAthenaGamePhase::SafeZones)
        RefreshCombatants(GameMode, Time);

    if ((EAthenaGamePhase)GamePhase == EAthenaGamePhase::Aircraft || (EAthenaGamePhase)GamePhase == EAthenaGamePhase::SafeZones)
        TickBus(Time);

    for (auto& Bot : AllBots)
    {
        if (Bot.State == EBotState::Dead)
            continue;

        auto Controller = GetWeak<ABotController>(Bot.Controller);
        auto Pawn = GetWeak<ABotPawn>(Bot.Pawn);

        if (!Controller || !Pawn || (Bot.State != EBotState::OnBus && IsPawnDead(Pawn)))
        {
            if (Pawn && !Pawn->bActorIsBeingDestroyed && Pawn->GetHealth() <= 0.f && !Pawn->IsDead()
                && (EAthenaGamePhase)GamePhase > EAthenaGamePhase::Warmup)
            {
                SafeForceKill(Pawn);
            }

            Bot.State = EBotState::Dead;
            Bot.DeathTime = Time;
            if ((EAthenaGamePhase)GamePhase > EAthenaGamePhase::Warmup)
                GuardedHandleElimination(Bot, Pawn);
            else
                RequestedBots--;
            continue;
        }

        KeepBotFallDamageOff(Pawn);

        if (Bot.State == EBotState::Skydiving)
            TickSkydive(Bot, Controller, Pawn, Time);

        if ((EAthenaGamePhase)GamePhase == EAthenaGamePhase::SafeZones && Bot.State == EBotState::Playing)
            TickStorm(Bot, Pawn, Time);

        if (Bot.State == EBotState::Playing)
        {
            TrackBotDamage(Bot, Pawn, Time);
            GuardedBrain(GameMode, &Bot, Controller, Pawn, GamePhase, Time);
        }
    }

    for (size_t i = 0; i < AllBots.size();)
    {
        if (AllBots[i].State == EBotState::Dead && Time - AllBots[i].DeathTime > 10.0)
        {
            AllBots.erase(AllBots.begin() + i);
            continue;
        }
        i++;
    }

    GuardedKnockbackAttributes(GameMode, Time);
    GuardedWatchPlayerFalls(GameMode, Time);
    LogBotStatus(GamePhase, Time);
    SyncPlayerCounts(GameMode, GamePhase);
    CheckForWinner(GameMode, GamePhase);
}

void Bots::NoteAttacker(AActor* VictimPawn, AActor* AttackerController)
{
    if (auto Bot = FindBotByPawn(VictimPawn))
    {
        Bot->LastAttacker = FWeakObjectPtr(AttackerController);
        Bot->LastHurtTime = Now();
    }
}
