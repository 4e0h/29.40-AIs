#include "pch.h"
#include "../Public/FortWeaponMods.h"
#include "../Public/FortPlayerControllerAthena.h"
#include "../Public/FortPlayerPawnAthena.h"
#include "../../Four_E_Zero_H/Public/Bots.h"
#include "../../Four_E_Zero_H/Plugins/CrashReporter/Public/CrashReporter.h"
#include <map>

using FMod = const UFortItemDefinition*;

enum EModCategory
{
    Category_Optic,
    Category_Barrel,
    Category_Underbarrel,
    Category_Magazine,
    Category_Count,
    Category_Unknown = -1
};

struct FSlotValue
{
    FMod Mod = nullptr;
    bool bDynamic = false;
};

static std::vector<FMod> AllMods;
static std::map<std::pair<FMod, const UFortItemDefinition*>, bool> AllowedCache;

static std::string Lower(std::string Text)
{
    std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char C) { return (char)tolower(C); });
    return Text;
}

static std::string NameOf(const UObject* Object)
{
    return Object ? std::string(Object->Name.ToString().c_str()) : std::string("none");
}

static int SlotSize()
{
    static int Size = FFortWeaponModSlot::StaticStruct() ? FFortWeaponModSlot::Size() : 0;
    return Size;
}

static bool Supported()
{
    static bool bSupported = SlotSize() > 0 && SlotSize() <= 0x40 && FFortItemEntry::HasWeaponModSlots() && FFortWeaponModSlot::HasWeaponMod();
    return bSupported;
}

static std::vector<FSlotValue> ReadSlots(const TArray<FFortWeaponModSlot>& Slots)
{
    std::vector<FSlotValue> Values;
    for (int i = 0; i < Slots.Num(); i++)
    {
        auto& Slot = Slots.Get(i, SlotSize());
        Values.push_back({ Slot.WeaponMod, FFortWeaponModSlot::HasbIsDynamic() && Slot.bIsDynamic });
    }
    return Values;
}

static void WriteSlots(TArray<FFortWeaponModSlot>& Slots, const std::vector<FSlotValue>& Values)
{
    int Size = SlotSize();
    if (Slots.Num() != (int)Values.size() || !Slots.Data)
    {
        Slots.Data = Values.empty() ? nullptr : (FFortWeaponModSlot*)FMemory::Malloc(Size * Values.size());
        Slots.NumElements = Slots.MaxElements = (int)Values.size();
    }

    for (int i = 0; i < (int)Values.size(); i++)
    {
        auto& Slot = Slots.Get(i, Size);
        memset((PBYTE)&Slot, 0, Size);
        Slot.GetWeaponMod() = Values[i].Mod;
        if (FFortWeaponModSlot::HasbIsDynamic())
            Slot.GetbIsDynamic() = Values[i].bDynamic;
    }
}

static bool SameMods(const std::vector<FSlotValue>& A, const std::vector<FSlotValue>& B)
{
    if (A.size() != B.size())
        return false;
    for (size_t i = 0; i < A.size(); i++)
        if (A[i].Mod != B[i].Mod)
            return false;
    return true;
}

static bool HasMod(const std::vector<FSlotValue>& Slots, FMod Mod)
{
    for (auto& Slot : Slots)
        if (Slot.Mod == Mod)
            return true;
    return false;
}

static bool IsDefaultMod(FMod Mod)
{
    return Mod && Lower(NameOf(Mod)).find("default") != std::string::npos;
}

static int Category(FMod Mod)
{
    if (!Mod)
        return Category_Unknown;

    auto Text = Lower(NameOf(Mod));
    auto Definition = (const UFortWeaponModItemDefinition*)Mod;
    if (Definition->HasModSlot())
        Text += " " + Lower(Definition->ModSlot.TagName.ToString().c_str());

    if (Text.find("optic") != std::string::npos || Text.find("sight") != std::string::npos || Text.find("scope") != std::string::npos)
        return Category_Optic;
    if (Text.find("underbarrel") != std::string::npos || Text.find("grip") != std::string::npos || Text.find("laser") != std::string::npos)
        return Category_Underbarrel;
    if (Text.find("barrel") != std::string::npos || Text.find("muzzle") != std::string::npos || Text.find("suppressor") != std::string::npos)
        return Category_Barrel;
    if (Text.find("magazine") != std::string::npos || Text.find("drum") != std::string::npos || Text.find("mag_") != std::string::npos)
        return Category_Magazine;
    return Category_Unknown;
}

static const char* CategoryName(int Category)
{
    switch (Category)
    {
    case Category_Optic:
        return "Optic";
    case Category_Barrel:
        return "Barrel";
    case Category_Underbarrel:
        return "Underbarrel";
    case Category_Magazine:
        return "Magazine";
    default:
        return "Other";
    }
}

static void Discover(bool bForce = false)
{
    static ULONGLONG NextScan = 0;
    static int Scans = 0;
    auto Now = GetTickCount64();
    if (!bForce && (Now < NextScan || Scans >= 10 || AllMods.size() >= 16))
        return;
    NextScan = Now + 60000;
    Scans++;

    auto ModClass = UFortWeaponModItemDefinition::StaticClass();
    if (!ModClass)
        return;

    std::vector<FMod> Found;
    for (int i = 0; i < TUObjectArray::Num(); i++)
    {
        auto Object = TUObjectArray::GetObjectByIndex(i);
        if (Object && !Object->IsDefaultObject() && Object->IsA(ModClass) && !IsDefaultMod((FMod)Object))
            Found.push_back((FMod)Object);
    }

    if (Found.size() > AllMods.size())
    {
        AllMods = Found;
        Bots::Log("[Mods] %d weapon mods available\n", (int)AllMods.size());
    }
}

static bool GuardedModAllowed(FMod Mod, const UFortItemDefinition* Weapon, bool* bAllowed)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        *bAllowed = UFortWeaponModFunctionLibrary::ModAllowedOnWeaponDefinition((UFortWeaponModItemDefinition*)Mod, (UFortItemDefinition*)Weapon);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;
    return bOk;
}

static bool GuardedTryAddWeaponMod(FMod Mod, AFortWeapon* Weapon, bool* bAdded)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        *bAdded = UFortWeaponModFunctionLibrary::TryAddWeaponMod((UFortWeaponModItemDefinition*)Mod, Weapon);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;
    return bOk;
}

static bool GuardedProcessEvent(UObject* Object, UFunction* Function, void* Params)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        Object->ProcessEvent(Function, Params);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;
    return bOk;
}

static bool HasFunction(const char* Name)
{
    auto Class = UFortWeaponModFunctionLibrary::StaticClass();
    return Class && UFortWeaponModFunctionLibrary::GetDefaultObj()->GetFunction(Name);
}

static bool ModAllowed(FMod Mod, const UFortItemDefinition* Weapon)
{
    static bool bNative = HasFunction("ModAllowedOnWeaponDefinition");
    if (!bNative || !Mod || !Weapon)
        return false;

    auto Key = std::make_pair(Mod, Weapon);
    auto Cached = AllowedCache.find(Key);
    if (Cached != AllowedCache.end())
        return Cached->second;

    bool bAllowed = false;
    if (!GuardedModAllowed(Mod, Weapon, &bAllowed))
        bAllowed = false;
    AllowedCache[Key] = bAllowed;
    return bAllowed;
}

static void PutMod(std::vector<FSlotValue>& Slots, FMod Mod)
{
    int Kind = Category(Mod);
    if (Kind != Category_Unknown)
        for (auto& Slot : Slots)
            if (Category(Slot.Mod) == Kind)
            {
                Slot = { Mod, true };
                return;
            }
    for (auto& Slot : Slots)
        if (!Slot.Mod)
        {
            Slot = { Mod, true };
            return;
        }
    Slots.push_back({ Mod, true });
}

static bool HasRealMods(const std::vector<FSlotValue>& Slots)
{
    for (auto& Slot : Slots)
        if (Slot.Mod && !IsDefaultMod(Slot.Mod))
            return true;
    return false;
}

static int RarityBudget(int Rarity)
{
    int Roll = rand() % 100;
    switch (Rarity)
    {
    case 0:
        return Roll < 30 ? 1 : 0;
    case 1:
        return Roll < 55 ? 1 : 0;
    case 2:
        return 1 + (Roll < 35 ? 1 : 0);
    case 3:
        return 1 + (Roll < 65 ? 1 : 0);
    case 4:
        return 2 + (Roll < 55 ? 1 : 0);
    default:
        return 3 + (Roll < 40 ? 1 : 0);
    }
}

static FFortItemEntry* FindReplicatedEntry(AFortInventory* Inventory, FGuid Guid)
{
    if (!Inventory)
        return nullptr;
    return Inventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& Entry) { return Entry.ItemGuid == Guid; }, FFortItemEntry::Size());
}

static UFortWorldItem* FindItem(AFortInventory* Inventory, FGuid Guid)
{
    if (!Inventory)
        return nullptr;
    auto Item = Inventory->Inventory.ItemInstances.Search([&](UFortWorldItem* Item) { return Item && Item->ItemEntry.ItemGuid == Guid; });
    return Item ? *Item : nullptr;
}

static void SetItemSlots(AFortInventory* Inventory, FGuid Guid, const std::vector<FSlotValue>& Slots)
{
    if (auto Item = FindItem(Inventory, Guid))
        WriteSlots(Item->ItemEntry.WeaponModSlots, Slots);
    if (auto Entry = FindReplicatedEntry(Inventory, Guid))
    {
        WriteSlots(Entry->WeaponModSlots, Slots);
        Inventory->Update(Entry);
    }
}

void WeaponMods::InitEntrySlots(const UFortItemDefinition* Definition, FFortItemEntry& Entry)
{
    auto WeaponDefinition = Definition ? Definition->Cast<UFortWeaponItemDefinition>() : nullptr;
    if (!Supported() || !WeaponDefinition || !WeaponDefinition->HasWeaponModSlots() || Entry.WeaponModSlots.Num() > 0)
        return;

    auto Defaults = ReadSlots(WeaponDefinition->WeaponModSlots);
    if (!Defaults.empty())
        WriteSlots(Entry.WeaponModSlots, Defaults);
}

void WeaponMods::OnItemGiven(AFortInventory* Inventory, FFortItemEntry& Source, UFortWorldItem* Item)
{
    if (!Supported() || !Inventory || !Item || &Source == &Item->ItemEntry)
        return;

    auto Slots = ReadSlots(Source.WeaponModSlots);
    if (!HasRealMods(Slots))
        return;

    SetItemSlots(Inventory, Item->ItemEntry.ItemGuid, Slots);
}

void WeaponMods::OnPickupSpawned(AFortPickupAthena* Pickup, FFortItemEntry& Source)
{
    if (!Supported() || !Pickup)
        return;

    auto& Entry = Pickup->PrimaryPickupItemEntry;
    auto Definition = Entry.ItemDefinition;
    auto WeaponDefinition = Definition ? Definition->Cast<UFortWeaponItemDefinition>() : nullptr;
    if (!WeaponDefinition || Definition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
        return;

    if (&Source != &Entry && Source.WeaponModSlots.Num() > 0)
    {
        WriteSlots(Entry.WeaponModSlots, ReadSlots(Source.WeaponModSlots));
        return;
    }

    InitEntrySlots(Definition, Entry);
    auto Slots = ReadSlots(Entry.WeaponModSlots);
    if (Slots.empty())
        return;

    Discover();
    int Budget = AllMods.empty() ? 0 : RarityBudget(Definition->Rarity);
    if (Budget <= 0)
        return;

    int Order[Category_Count] = { Category_Optic, Category_Barrel, Category_Underbarrel, Category_Magazine };
    for (int i = Category_Count - 1; i > 0; i--)
        std::swap(Order[i], Order[rand() % (i + 1)]);

    int Added = 0;
    for (int i = 0; i < Category_Count && Added < Budget; i++)
    {
        std::vector<FMod> Pool;
        for (auto Mod : AllMods)
            if (Category(Mod) == Order[i] && ModAllowed(Mod, Definition))
                Pool.push_back(Mod);
        if (Pool.empty())
            continue;
        PutMod(Slots, Pool[rand() % Pool.size()]);
        Added++;
    }

    if (Added > 0)
        WriteSlots(Entry.WeaponModSlots, Slots);
}

static bool SetWeaponSlots(AFortWeapon* Weapon, const std::vector<FSlotValue>& Slots)
{
    if (!Weapon || !Weapon->HasWeaponModSlots())
        return false;

    auto& WeaponSlots = Weapon->WeaponModSlots;
    auto Previous = ReadSlots(WeaponSlots);

    TArray<FFortWeaponModSlot> PreviousArray{};
    WriteSlots(PreviousArray, Previous);
    WriteSlots(WeaponSlots, Slots);

    static auto OnRep = Weapon->GetFunction("OnRep_ReplicatedWeaponModSlots");
    bool bOk = !OnRep || GuardedProcessEvent(Weapon, OnRep, &PreviousArray);
    if (PreviousArray.Data)
        FMemory::Free(PreviousArray.Data);

    Weapon->ForceNetUpdate();
    return bOk;
}

static bool AddModToWeapon(AFortWeapon* Weapon, FMod Mod)
{
    if (!Weapon || !Mod || !Weapon->HasWeaponModSlots())
        return false;

    static bool bNative = HasFunction("TryAddWeaponMod");
    bool bAdded = false;
    if (bNative && !GuardedTryAddWeaponMod(Mod, Weapon, &bAdded))
        Bots::Log("[Mods] TryAddWeaponMod faulted for %s\n", NameOf(Mod).c_str());

    auto Slots = ReadSlots(Weapon->WeaponModSlots);
    if (HasMod(Slots, Mod))
        return true;

    PutMod(Slots, Mod);
    return SetWeaponSlots(Weapon, Slots) && HasMod(ReadSlots(Weapon->WeaponModSlots), Mod);
}

void WeaponMods::OnWeaponEquipped(AFortPlayerControllerAthena* PlayerController, AActor* WeaponActor)
{
    auto Weapon = (AFortWeapon*)WeaponActor;
    if (!Supported() || !PlayerController || !Weapon || !Weapon->IsA<AFortWeapon>() || !Weapon->HasWeaponModSlots())
        return;

    auto Entry = FindReplicatedEntry(PlayerController->WorldInventory, Weapon->ItemEntryGuid);
    if (!Entry)
        return;

    auto Wanted = ReadSlots(Entry->WeaponModSlots);
    if (Wanted.empty() || SameMods(Wanted, ReadSlots(Weapon->WeaponModSlots)))
        return;

    SetWeaponSlots(Weapon, Wanted);
}

static void (*ServerPurchaseWeaponModForWeaponOG)(UObject*, FFrame&, void*) = nullptr;

static bool GuardedPurchase(UObject* Context, FFrame& Stack, void* Result)
{
    bool bOk = true;
    GCrashGuardDepth++;
    __try
    {
        ServerPurchaseWeaponModForWeaponOG(Context, Stack, Result);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        bOk = false;
    }
    GCrashGuardDepth--;
    return bOk;
}

static void ServerPurchaseWeaponModForWeapon(UObject* Context, FFrame& Stack, void* Result)
{
    auto Mod = Stack.Locals ? *(FMod*)Stack.Locals : nullptr;
    auto Weapon = Stack.Locals ? *(AFortWeapon**)(Stack.Locals + 8) : nullptr;

    bool bGameRan = ServerPurchaseWeaponModForWeaponOG && GuardedPurchase(Context, Stack, Result);

    if (!Supported() || !Mod || !Weapon || !Weapon->IsA<AFortWeapon>())
        return;
    auto Pawn = (AFortPlayerPawnAthena*)Weapon->Instigator;
    auto PlayerController = Pawn ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;
    if (!PlayerController || !PlayerController->IsA<AFortPlayerControllerAthena>())
        return;

    bool bByGame = HasMod(ReadSlots(Weapon->WeaponModSlots), Mod);
    bool bOn = bByGame || AddModToWeapon(Weapon, Mod);
    if (bOn)
        SetItemSlots(PlayerController->WorldInventory, Weapon->ItemEntryGuid, ReadSlots(Weapon->WeaponModSlots));

    Bots::Log("[Mods] bench: %s on %s - %s\n", NameOf(Mod).c_str(), NameOf(Weapon->WeaponData).c_str(),
              bByGame ? "added by the game" : bOn ? "added by the server" : (bGameRan ? "failed" : "failed (game's purchase faulted)"));
}

void WeaponMods::Hook()
{
    auto StationClass = FindClass("FortWeaponModStationComponent");
    auto Function = StationClass ? StationClass->GetDefaultObj()->GetFunction("ServerPurchaseWeaponModForWeapon") : nullptr;
    if (!Function)
    {
        Bots::Log("[Mods] no mod bench purchase function on this build\n");
        return;
    }
    Hooking::ExecHook(Function, ServerPurchaseWeaponModForWeapon, ServerPurchaseWeaponModForWeaponOG);
}

static FMod FindModByName(const std::string& Wanted)
{
    static const std::pair<const char*, const char*> Aliases[] = {
        { "reddot", "reddot" }, { "holo", "holo" }, { "p2x", "p2x" }, { "acog", "p2x" }, { "sniper", "optic_sniper" }, { "scope", "optic_sniper" },
        { "iron", "ironsights" }, { "suppressor", "suppressor" }, { "silencer", "suppressor" }, { "muzzle", "muzzlebrake" },
        { "vert", "vertforegrip" }, { "vertical", "vertforegrip" }, { "angled", "angledforegrip" }, { "laser", "laser" },
        { "speed", "magazine_speed" }, { "speedmag", "magazine_speed" }, { "drum", "magazine_drum" }, { "drummag", "magazine_drum" },
    };

    auto Search = Lower(Wanted);
    for (auto& [Alias, Match] : Aliases)
        if (Search == Alias)
            Search = Match;

    for (auto Mod : AllMods)
        if (Lower(NameOf(Mod)).find(Search) != std::string::npos)
            return Mod;
    return nullptr;
}

static void Say(AFortPlayerControllerAthena* PlayerController, const std::string& Text)
{
    Bots::Log("[Mods] %s\n", Text.c_str());
    PlayerController->ClientMessage(FString(std::wstring(Text.begin(), Text.end()).c_str()), FName(), 5.f);
}

bool WeaponMods::HandleCheat(AFortPlayerControllerAthena* PlayerController, const std::string& Command, const std::string& Argument)
{
    if (Command != "mod" && Command != "mods")
        return false;

    if (!Supported())
    {
        Say(PlayerController, "This build has no weapon mods.");
        return true;
    }
    Discover(true);

    if (Command == "mod" && (Argument.empty() || Argument == "list"))
    {
        std::string Text = "cheat mod <name> | all | rand | clear";
        for (int Kind = 0; Kind < Category_Count; Kind++)
        {
            std::string Line;
            for (auto Mod : AllMods)
                if (Category(Mod) == Kind)
                    Line += (Line.empty() ? "" : ", ") + NameOf(Mod);
            if (!Line.empty())
                Text += std::string("\n") + CategoryName(Kind) + ": " + Line;
        }
        Say(PlayerController, Text);
        return true;
    }

    auto Pawn = (AFortPlayerPawnAthena*)PlayerController->MyFortPawn;
    auto Weapon = Pawn ? (AFortWeapon*)Pawn->CurrentWeapon : nullptr;
    if (!Weapon || !Weapon->IsA<AFortWeapon>() || !Weapon->HasWeaponModSlots())
    {
        Say(PlayerController, "Hold a weapon first!");
        return true;
    }

    auto Summary = [&]()
    {
        std::string Text;
        for (auto& Slot : ReadSlots(Weapon->WeaponModSlots))
            Text += (Text.empty() ? "" : ", ") + NameOf(Slot.Mod);
        return Text.empty() ? std::string("no mod slots") : Text;
    };

    if (Command == "mods")
    {
        Say(PlayerController, NameOf(Weapon->WeaponData) + ": " + Summary());
        return true;
    }

    std::vector<FMod> ToAdd;
    if (Argument == "clear")
    {
        auto Defaults = std::vector<FSlotValue>{};
        if (Weapon->WeaponData && Weapon->WeaponData->HasWeaponModSlots())
            Defaults = ReadSlots(Weapon->WeaponData->WeaponModSlots);
        SetWeaponSlots(Weapon, Defaults);
    }
    else if (Argument == "all" || Argument == "rand" || Argument == "random")
    {
        for (int Kind = 0; Kind < Category_Count; Kind++)
        {
            if (Argument != "all" && rand() % 3 == 0)
                continue;
            std::vector<FMod> Pool;
            for (auto Mod : AllMods)
                if (Category(Mod) == Kind && ModAllowed(Mod, Weapon->WeaponData))
                    Pool.push_back(Mod);
            if (!Pool.empty())
                ToAdd.push_back(Pool[rand() % Pool.size()]);
        }
    }
    else if (auto Mod = FindModByName(Argument))
        ToAdd.push_back(Mod);
    else
    {
        Say(PlayerController, "No mod matches '" + Argument + "', try 'cheat mod list'.");
        return true;
    }

    for (auto Mod : ToAdd)
        AddModToWeapon(Weapon, Mod);
    SetItemSlots(PlayerController->WorldInventory, Weapon->ItemEntryGuid, ReadSlots(Weapon->WeaponModSlots));
    Say(PlayerController, NameOf(Weapon->WeaponData) + ": " + Summary());
    return true;
}
