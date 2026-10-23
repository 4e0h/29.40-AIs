#pragma once
#include "../../pch.h"

class Bots
{
public:
    static void Tick();

    static AActor* SpawnBot(FVector Location);

    static void OnPickup(AActor* Controller, AActor* Pawn, struct FFortItemEntry* Entry);

    static bool WantsPickup(AActor* Controller, AActor* Pickup);


    static void NoteAttacker(AActor* VictimPawn, AActor* AttackerController);

    static void Log(const char* Format, ...);

    static bool IsBotController(const UObject* Controller);
    static int32 GetAliveBotCount();
    static int32 GetSpawnedBotCount();
};
