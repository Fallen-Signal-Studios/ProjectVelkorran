// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionPIEInputLibrary.h"
#include "Framework/SovPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "UObject/Package.h"

FSovAurelionPIEMouseInputResult USovAurelionPIEInputLibrary::InjectAurelionPIEMouseDelta(
    UWorld* World, float DeltaX, float DeltaY)
{
    FSovAurelionPIEMouseInputResult Result;
    if (!IsInGameThread() || !GEditor || !GEngine || !IsValid(World)
        || World->WorldType != EWorldType::PIE || GEditor->PlayWorld != World
        || GEditor->IsSimulateInEditorInProgress())
    { Result.Report = TEXT("Mouse input requires the current real Aurelion PIE world on the game thread."); return Result; }

    const FString Package = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    if (Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13"))
    { Result.Report = TEXT("Mouse input is restricted to the exact M12 and M13 Aurelion wrappers."); return Result; }
    int32 PIEWorldCount = 0;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    { if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) { ++PIEWorldCount; } }
    if (PIEWorldCount != 1 || !FMath::IsFinite(DeltaX) || !FMath::IsFinite(DeltaY)
        || FMath::Abs(DeltaX) > 512.f || FMath::Abs(DeltaY) > 512.f)
    { Result.Report = TEXT("Requires one PIE world and finite mouse deltas within +/-512 per sample."); return Result; }

    UGameInstance* const GameInstance = World->GetGameInstance();
    if (!IsValid(GameInstance) || GameInstance->GetNumLocalPlayers() != 1)
    { Result.Report = TEXT("Requires exactly one local player in the current PIE game instance."); return Result; }
    ULocalPlayer* const LocalPlayer = GameInstance->GetLocalPlayers()[0];
    ASovPlayerController* const Controller = IsValid(LocalPlayer)
        ? Cast<ASovPlayerController>(LocalPlayer->GetPlayerController(World)) : nullptr;
    int32 PlayerControllerCount = 0;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
    { if (IsValid(It->Get())) { ++PlayerControllerCount; } }
    if (PlayerControllerCount != 1 || !IsValid(Controller) || Controller->IsActorBeingDestroyed()
        || Controller->GetWorld() != World || !Controller->IsLocalController()
        || Controller->GetLocalPlayer() != LocalPlayer || !IsValid(Controller->PlayerInput)
        || !IsValid(LocalPlayer->ViewportClient) || !LocalPlayer->ViewportClient->Viewport
        || LocalPlayer->ViewportClient->GetWorld() != World)
    { Result.Report = TEXT("Requires the sole current local Sovereign controller, player input and PIE viewport."); return Result; }

    IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
    const FInputDeviceId Device = DeviceMapper.GetPrimaryInputDeviceForUser(LocalPlayer->GetPlatformUserId());
    if (!Device.IsValid() || DeviceMapper.GetUserForInputDevice(Device) != LocalPlayer->GetPlatformUserId())
    { Result.Report = TEXT("The local player has no valid owned input device; input routing was not changed."); return Result; }
    const double FrameDelta = FApp::GetDeltaTime();
    if (!FMath::IsFinite(FrameDelta) || FrameDelta <= 0.)
    { Result.Report = TEXT("Requires a finite positive editor frame interval."); return Result; }

    UPlayerInput* const ExpectedInput = Controller->PlayerInput;
    UGameViewportClient* const ExpectedViewport = LocalPlayer->ViewportClient;
    Result.ControllerPath = Controller->GetPathName();
    const auto StillCurrent = [&]()
    {
        return IsValid(World) && GEditor && GEditor->PlayWorld == World
            && World->GetGameInstance() == GameInstance && IsValid(GameInstance)
            && GameInstance->GetNumLocalPlayers() == 1 && GameInstance->GetLocalPlayers()[0] == LocalPlayer
            && IsValid(LocalPlayer) && LocalPlayer->GetPlayerController(World) == Controller
            && IsValid(Controller) && !Controller->IsActorBeingDestroyed() && Controller->GetWorld() == World
            && Controller->GetLocalPlayer() == LocalPlayer && Controller->IsLocalController()
            && IsValid(ExpectedInput) && Controller->PlayerInput == ExpectedInput
            && IsValid(ExpectedViewport) && LocalPlayer->ViewportClient == ExpectedViewport
            && ExpectedViewport->GetWorld() == World && ExpectedViewport->Viewport;
    };
    FInputKeyEventArgs MouseX = FInputKeyEventArgs::CreateSimulated(
        EKeys::MouseX, IE_Axis, DeltaX, 1, Device, false, ExpectedViewport->Viewport);
    FInputKeyEventArgs MouseY = FInputKeyEventArgs::CreateSimulated(
        EKeys::MouseY, IE_Axis, DeltaY, 1, Device, false, ExpectedViewport->Viewport);
    MouseX.DeltaTime = MouseY.DeltaTime = static_cast<float>(FMath::Clamp(FrameDelta, .001, .25));

    Result.bMouseXConsumed = Controller->InputKey(MouseX);
    if (!StillCurrent())
    { Result.Report = TEXT("Input ownership changed after MouseX; MouseY was not routed."); return Result; }
    Result.bMouseYConsumed = Controller->InputKey(MouseY);
    Result.bRouted = StillCurrent();
    Result.Report = Result.bRouted
        ? TEXT("Synthetic mouse samples routed through ordinary InputKey. Observe the following input frame and actual wheel selection; analog consumed flags may be false.")
        : TEXT("Input ownership changed after MouseY; the requested pair cannot be qualified.");
    return Result;
}
