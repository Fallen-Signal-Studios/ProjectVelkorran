// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/NarrativeAIStartupDiagnostics.h"

#if !UE_BUILD_SHIPPING
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "ArsenalStatics.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeGameState.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovAIStartup, Log, All);

namespace NarrativeAIStartupTrace
{
    // Hard ceilings prevent a forgotten switch or a perception storm flooding logs.
    constexpr double CaptureSeconds = 120.0;
    constexpr int32 MaximumEvents = 2000;
    constexpr int32 MaximumControllers = 32;
    constexpr int32 MaximumItems = 32;
    TAutoConsoleVariable<int32> Enabled(TEXT("sov.AIStartupTrace"), 0,
        TEXT("Read-only startup trace. Set 1 BEFORE PIE/new world; 0 stops. Non-Shipping only. 120s/2000 events per world."), ECVF_Default);

    struct FControllerSample
    {
        double NextSample = 0.;
        double NextHeartbeat = 0.;
        FString Previous;
    };
    struct FCapture
    {
        double Started = 0.;
        uint64 Id = 0;
        int32 Sequence = 0;
        bool bStopped = false;
        TMap<TWeakObjectPtr<ANarrativeNPCController>, FControllerSample> Controllers;
    };
    TMap<TWeakObjectPtr<UWorld>, FCapture> Captures;
    FDelegateHandle WorldInitHandle;
    FDelegateHandle WorldCleanupHandle;
    uint64 NextId = 0;

    FString Serialize(const TSharedRef<FJsonObject>& Object)
    {
        FString Result;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
        FJsonSerializer::Serialize(Object, Writer);
        return Result;
    }

    void Emit(UWorld* World, FCapture& Capture, const UObject* Context,
        const TCHAR* Event, const TSharedRef<FJsonObject>& Data)
    {
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("schema"), 1);
        Row->SetStringField(TEXT("capture"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Capture.Id)));
        Row->SetNumberField(TEXT("sequence"), ++Capture.Sequence);
        Row->SetStringField(TEXT("world"), World->GetPathName());
        Row->SetNumberField(TEXT("net_mode"), static_cast<int32>(World->GetNetMode()));
        Row->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Capture.Started);
        Row->SetNumberField(TEXT("game_seconds"), World->GetTimeSeconds());
        Row->SetStringField(TEXT("context"), GetPathNameSafe(Context));
        Row->SetStringField(TEXT("event"), Event);
        Row->SetObjectField(TEXT("data"), Data);
        UE_LOG(LogSovAIStartup, Log, TEXT("SOV_AI_STARTUP %s"), *Serialize(Row));
    }

    FCapture* Active(const UObject* Context)
    {
        UWorld* World = Context ? Context->GetWorld() : nullptr;
        if (!World || !IsInGameThread()) { return nullptr; }
        FCapture* Capture = Captures.Find(World);
        // A late switch cannot retroactively observe startup.
        if (!Capture || Capture->bStopped) { return nullptr; }
        const TCHAR* StopReason = nullptr;
        if (!Enabled.GetValueOnGameThread()) { StopReason = TEXT("disabled"); }
        else if (FPlatformTime::Seconds() - Capture->Started >= CaptureSeconds) { StopReason = TEXT("time_limit"); }
        else if (Capture->Sequence >= MaximumEvents - 1) { StopReason = TEXT("event_limit"); }
        if (StopReason)
        {
            const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
            Data->SetStringField(TEXT("reason"), StopReason);
            Emit(World, *Capture, World, TEXT("capture_stopped"), Data);
            Capture->bStopped = true;
            Capture->Controllers.Empty();
            return nullptr;
        }
        return Capture;
    }

    TSharedRef<FJsonObject> PawnState(APawn* Pawn)
    {
        const TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
        State->SetStringField(TEXT("pawn"), GetPathNameSafe(Pawn));
        State->SetBoolField(TEXT("begun_play"), Pawn && Pawn->HasActorBegunPlay());
        const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Pawn);
        State->SetBoolField(TEXT("narrative_character"), Character != nullptr);
        State->SetBoolField(TEXT("alive"), Character && Character->IsAlive());
        State->SetStringField(TEXT("factions"), Character ? Character->GetFactions().ToString() : FString());
        const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
        State->SetStringField(TEXT("asc"), GetPathNameSafe(ASC));
        State->SetStringField(TEXT("asc_owner"), ASC ? GetPathNameSafe(ASC->GetOwnerActor()) : TEXT("None"));
        State->SetStringField(TEXT("asc_avatar"), ASC ? GetPathNameSafe(ASC->GetAvatarActor()) : TEXT("None"));
        State->SetBoolField(TEXT("asc_avatar_matches"), ASC && Pawn && ASC->GetAvatarActor() == Pawn);
        const UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
        State->SetNumberField(TEXT("asc_ready_epoch"), NarrativeASC ? NarrativeASC->GetCharacterReadyEpoch() : 0);
        return State;
    }

    void WorldInitialized(UWorld* World, const UWorld::InitializationValues)
    {
        if (!IsInGameThread() || !Enabled.GetValueOnGameThread() || !World
            || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)) { return; }
        FCapture& Capture = Captures.Add(World);
        Capture.Started = FPlatformTime::Seconds();
        Capture.Id = ++NextId;
        const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("attachment"), TEXT("OnPreWorldInitialization"));
        Data->SetStringField(TEXT("before_perception_binding"), TEXT("unknown; see perception_attached for each controller"));
        Data->SetNumberField(TEXT("maximum_seconds"), CaptureSeconds);
        Data->SetNumberField(TEXT("maximum_events"), MaximumEvents);
        Emit(World, Capture, World, TEXT("observer_armed"), Data);
    }

    void WorldCleanup(UWorld* World, bool, bool)
    {
        if (FCapture* Capture = Active(World))
        {
            const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
            Data->SetStringField(TEXT("reason"), TEXT("world_cleanup"));
            Emit(World, *Capture, World, TEXT("capture_stopped"), Data);
        }
        Captures.Remove(World);
    }
}

void FNarrativeAIStartupDiagnostics::Startup()
{
    using namespace NarrativeAIStartupTrace;
    WorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&WorldInitialized);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&WorldCleanup);
}

void FNarrativeAIStartupDiagnostics::Shutdown()
{
    using namespace NarrativeAIStartupTrace;
    FWorldDelegates::OnPreWorldInitialization.Remove(WorldInitHandle);
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    Captures.Empty();
}

void FNarrativeAIStartupDiagnostics::Record(const UObject* Context, const TCHAR* Event, const FString& Detail)
{
    using namespace NarrativeAIStartupTrace;
    if (FCapture* Capture = Active(Context))
    {
        const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("detail"), Detail);
        Emit(Context->GetWorld(), *Capture, Context, Event, Data);
    }
}

void FNarrativeAIStartupDiagnostics::Perception(ANarrativeNPCController* Controller, AActor* Target, const FAIStimulus& Stimulus)
{
    using namespace NarrativeAIStartupTrace;
    if (FCapture* Capture = Active(Controller))
    {
        const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("target"), GetPathNameSafe(Target));
        Data->SetBoolField(TEXT("sight"), Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>());
        Data->SetBoolField(TEXT("success"), Stimulus.WasSuccessfullySensed());
        Data->SetBoolField(TEXT("expired"), Stimulus.IsExpired());
        Data->SetNumberField(TEXT("stimulus_age"), Stimulus.GetAge());
        Data->SetObjectField(TEXT("target_state"), PawnState(Cast<APawn>(Target)));
        Data->SetNumberField(TEXT("attitude"), static_cast<int32>(UArsenalStatics::GetAttitude(Controller, Target)));
        Emit(Controller->GetWorld(), *Capture, Controller, TEXT("perception_callback"), Data);
        Snapshot(Controller, true);
    }
}

void FNarrativeAIStartupDiagnostics::Snapshot(ANarrativeNPCController* Controller, bool bForce)
{
    using namespace NarrativeAIStartupTrace;
    FCapture* Capture = Active(Controller);
    if (!Capture || !Controller) { return; }
    FControllerSample* Sample = Capture->Controllers.Find(Controller);
    if (!Sample)
    {
        if (Capture->Controllers.Num() >= MaximumControllers) { return; }
        Sample = &Capture->Controllers.Add(Controller);
    }
    const double Now = FPlatformTime::Seconds();
    if (!bForce && Now < Sample->NextSample) { return; }
    Sample->NextSample = Now + .25;
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("controller_begun_play"), Controller->HasActorBegunPlay());
    Data->SetBoolField(TEXT("authority"), Controller->HasAuthority());
    Data->SetObjectField(TEXT("controlled"), PawnState(Controller->GetPawn()));
    Data->SetStringField(TEXT("game_state"), GetPathNameSafe(Controller->GetWorld()->GetGameState()));
    Data->SetBoolField(TEXT("narrative_game_state"), Cast<ANarrativeGameState>(Controller->GetWorld()->GetGameState()) != nullptr);
    Data->SetStringField(TEXT("tree"), GetPathNameSafe(Controller->GetCurrentTree()));

    const UAIPerceptionComponent* Perception = Controller->GetPerceptionComponent();
    Data->SetStringField(TEXT("perception"), GetPathNameSafe(Perception));
    Data->SetBoolField(TEXT("perception_active"), Perception && Perception->IsActive());
    TArray<AActor*> Sight;
    if (Perception) { Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Sight); }
    TArray<FString> SightPaths;
    for (const AActor* Actor : Sight) { SightPaths.Add(GetPathNameSafe(Actor)); }
    SightPaths.Sort();
    TArray<TSharedPtr<FJsonValue>> SightJSON;
    for (int32 I = 0; I < FMath::Min(SightPaths.Num(), MaximumItems); ++I)
    { SightJSON.Add(MakeShared<FJsonValueString>(SightPaths[I])); }
    Data->SetArrayField(TEXT("current_sight"), SightJSON);
    Data->SetNumberField(TEXT("current_sight_count"), Sight.Num());

    TArray<TSharedPtr<FJsonValue>> Players;
    int32 PlayerCount = 0;
    for (FConstPlayerControllerIterator It = Controller->GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PlayerController = It->Get();
        if (!PlayerController) { continue; }
        ++PlayerCount;
        if (Players.Num() >= 4) { continue; }
        APawn* Pawn = PlayerController->GetPawn();
        const TSharedRef<FJsonObject> Player = PawnState(Pawn);
        Player->SetStringField(TEXT("controller"), PlayerController->GetPathName());
        Player->SetNumberField(TEXT("attitude"), static_cast<int32>(UArsenalStatics::GetAttitude(Controller, Pawn)));
        Player->SetBoolField(TEXT("currently_seen"), Pawn && Sight.Contains(Pawn));
        Players.Add(MakeShared<FJsonValueObject>(Player));
    }
    Data->SetArrayField(TEXT("players"), Players);
    Data->SetNumberField(TEXT("player_count"), PlayerCount);

    const UNPCActivityComponent* Activity = Controller->GetActivityComponent();
    Data->SetStringField(TEXT("activity_component"), GetPathNameSafe(Activity));
    Data->SetBoolField(TEXT("activity_active"), Activity && Activity->IsActive());
    Data->SetStringField(TEXT("activity"), Activity ? GetPathNameSafe(Activity->CurrentActivity) : TEXT("None"));
    TArray<TSharedPtr<FJsonValue>> Generators;
    TArray<TSharedPtr<FJsonValue>> GoalTypes;
    int32 GoalCount = 0;
    if (Activity)
    {
        Data->SetStringField(TEXT("activity_owner_controller"), GetPathNameSafe(Activity->OwnerController));
        Data->SetNumberField(TEXT("generator_count"), Activity->GoalGenerators.Num());
        for (const auto& Generator : Activity->GoalGenerators)
        {
            if (Generators.Num() >= MaximumItems) { break; }
            const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("generator"), GetPathNameSafe(Generator));
            Item->SetStringField(TEXT("outer"), Generator ? GetPathNameSafe(Generator->GetOuter()) : TEXT("None"));
            Generators.Add(MakeShared<FJsonValueObject>(Item));
        }
        for (const auto& Pair : Activity->Goals)
        {
            GoalCount += Pair.Value.Goals.Num();
            if (GoalTypes.Num() >= MaximumItems) { continue; }
            const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("class"), GetPathNameSafe(Pair.Key.Get()));
            Item->SetNumberField(TEXT("count"), Pair.Value.Goals.Num());
            GoalTypes.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    Data->SetArrayField(TEXT("generators"), Generators);
    Data->SetArrayField(TEXT("goal_types"), GoalTypes);
    Data->SetNumberField(TEXT("goal_count"), GoalCount);
    const FString Signature = Serialize(Data);
    if (bForce || Signature != Sample->Previous || Now >= Sample->NextHeartbeat)
    {
        Sample->Previous = Signature;
        Sample->NextHeartbeat = Now + 5.;
        Emit(Controller->GetWorld(), *Capture, Controller, TEXT("snapshot"), Data);
    }
}
#endif
