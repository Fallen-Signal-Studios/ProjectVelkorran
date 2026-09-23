// Copyright Fallen Signal Studios. All Rights Reserved.
#include "SovAurelionPIEInputLibrary.h"
#include "Framework/SovPlayerController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

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

FSovAurelionPIEPointerInputResult USovAurelionPIEInputLibrary::InjectAurelionPIELeftClick(UWorld* World)
{
    FSovAurelionPIEPointerInputResult Result;
    if (!IsInGameThread() || !GEditor || !GEngine || !FSlateApplication::IsInitialized() || !IsValid(World)
        || World->WorldType != EWorldType::PIE || GEditor->PlayWorld != World || GEditor->IsSimulateInEditorInProgress())
    { Result.Report = TEXT("Pointer input requires the current real Aurelion PIE world and Slate on the game thread."); return Result; }
    const FString Package = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    if (Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13"))
    { Result.Report = TEXT("Pointer input is restricted to the exact M12 and M13 Aurelion wrappers."); return Result; }
    int32 PIEWorldCount = 0;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    { if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) { ++PIEWorldCount; } }
    UGameInstance* const GameInstance = World->GetGameInstance();
    if (PIEWorldCount != 1 || !IsValid(GameInstance) || GameInstance->GetNumLocalPlayers() != 1)
    { Result.Report = TEXT("Requires one PIE world with exactly one local player."); return Result; }
    ULocalPlayer* const LocalPlayer = GameInstance->GetLocalPlayers()[0];
    ASovPlayerController* const Controller = IsValid(LocalPlayer)
        ? Cast<ASovPlayerController>(LocalPlayer->GetPlayerController(World)) : nullptr;
    UGameViewportClient* const ViewportClient = IsValid(LocalPlayer) ? LocalPlayer->ViewportClient.Get() : nullptr;
    const TSharedPtr<SViewport> ViewportWidget = IsValid(ViewportClient) ? ViewportClient->GetGameViewportWidget() : nullptr;
    if (!IsValid(Controller) || Controller->IsActorBeingDestroyed() || !Controller->IsLocalController()
        || !IsValid(ViewportClient) || ViewportClient->GetWorld() != World || !ViewportWidget.IsValid())
    { Result.Report = TEXT("Requires the sole current local Sovereign controller and its PIE viewport widget."); return Result; }
    const FGeometry& Geometry = ViewportWidget->GetTickSpaceGeometry();
    const FVector2D Size(Geometry.GetLocalSize());
    if (Size.X < 2. || Size.Y < 2.)
    { Result.Report = TEXT("The PIE viewport has no arranged screen geometry."); return Result; }
    Result.ScreenPosition = FVector2D(Geometry.LocalToAbsolute(Size * .5));
    const auto StillCurrent = [&]()
    {
        return IsValid(World) && GEditor && GEditor->PlayWorld == World && IsValid(LocalPlayer)
            && LocalPlayer->GetPlayerController(World) == Controller && IsValid(Controller) && !Controller->IsActorBeingDestroyed()
            && LocalPlayer->ViewportClient == ViewportClient && ViewportClient->GetGameViewportWidget() == ViewportWidget;
    };
    FSlateApplication& Slate = FSlateApplication::Get();
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const FPointerEvent Press(FSlateApplicationBase::CursorPointerIndex, Result.ScreenPosition, Result.ScreenPosition,
        Pressed, EKeys::LeftMouseButton, 0.f, Slate.GetModifierKeys());
    Result.bPressHandled = Slate.ProcessMouseButtonDownEvent(nullptr, Press);
    if (!StillCurrent())
    { Result.Report = TEXT("Viewport ownership changed after press; release was still sent to avoid a held button.");
      Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplicationBase::CursorPointerIndex, Result.ScreenPosition,
          Result.ScreenPosition, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, Slate.GetModifierKeys())); return Result; }
    const FPointerEvent Release(FSlateApplicationBase::CursorPointerIndex, Result.ScreenPosition, Result.ScreenPosition,
        TSet<FKey>(), EKeys::LeftMouseButton, 0.f, Slate.GetModifierKeys());
    Result.bReleaseHandled = Slate.ProcessMouseButtonUpEvent(Release);
    Result.bRouted = StillCurrent();
    Result.Report = Result.bRouted
        ? TEXT("Left press and release routed through Slate at the PIE viewport centre. Observe the actual UI result; handled flags depend on the receiving widget.")
        : TEXT("Viewport ownership changed after release; the click cannot be qualified.");
    return Result;
}

FSovAurelionPIEViewportCaptureResult USovAurelionPIEInputLibrary::CaptureAurelionPIEViewport(
    UWorld* World, const FString& Filename)
{
    FSovAurelionPIEViewportCaptureResult Result;
    if (!IsInGameThread() || !GEditor || !GEngine || !IsValid(World)
        || World->WorldType != EWorldType::PIE || GEditor->PlayWorld != World
        || GEditor->IsSimulateInEditorInProgress())
    { Result.Report = TEXT("Capture requires the current real Aurelion PIE world on the game thread."); return Result; }
    const FString Package = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    if (Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M12") && Package != TEXT("/Game/Aurelion/Maps/L_Aurelion_M13"))
    { Result.Report = TEXT("Capture is restricted to the exact M12 and M13 Aurelion wrappers."); return Result; }
    int32 PIEWorldCount = 0;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    { if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) { ++PIEWorldCount; } }
    UGameInstance* const Instance = World->GetGameInstance();
    if (PIEWorldCount != 1 || !IsValid(Instance) || Instance->GetNumLocalPlayers() != 1)
    { Result.Report = TEXT("Capture requires one PIE world and one local player."); return Result; }
    ULocalPlayer* const Player = Instance->GetLocalPlayers()[0];
    UGameViewportClient* const Client = IsValid(Player) ? Player->ViewportClient.Get() : nullptr;
    ASovPlayerController* const Controller = IsValid(Player)
        ? Cast<ASovPlayerController>(Player->GetPlayerController(World)) : nullptr;
    FViewport* const Viewport = IsValid(Client) ? Client->Viewport : nullptr;
    if (!IsValid(Controller) || !Controller->IsLocalController() || Controller->GetWorld() != World
        || !IsValid(Client) || Client->GetWorld() != World || !Viewport)
    { Result.Report = TEXT("Capture could not resolve the sole local player's game viewport."); return Result; }
    FString Full = FPaths::ConvertRelativePathToFull(Filename);
    FString Root = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Validation/Aurelion")));
    FPaths::NormalizeFilename(Full);
    FPaths::NormalizeFilename(Root);
    if (!FPaths::IsUnderDirectory(Full, Root) || !FPaths::GetExtension(Full).Equals(TEXT("png"), ESearchCase::IgnoreCase))
    { Result.Report = FString::Printf(TEXT("Capture requires a PNG under %s; received %s."), *Root, *Full); return Result; }
    const FIntPoint Size = Viewport->GetSizeXY();
    if (Size.X < 16 || Size.Y < 16 || Size.X > 8192 || Size.Y > 8192)
    { Result.Report = TEXT("The player viewport has no usable back buffer dimensions."); return Result; }
    TArray<FColor> Pixels;
    if (!Viewport->ReadPixels(Pixels) || Pixels.Num() != Size.X * Size.Y)
    { Result.Report = TEXT("The player viewport back buffer could not be read."); return Result; }
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Full), true)
        || !FImageUtils::SaveImageByExtension(*Full, FImageView(Pixels.GetData(), Size.X, Size.Y)))
    { Result.Report = TEXT("The player viewport PNG could not be saved."); return Result; }
    Result.bCaptured = true;
    Result.Filename = Full;
    Result.Width = Size.X;
    Result.Height = Size.Y;
    Result.Report = TEXT("Captured the validated local player's PIE back buffer; Slate HUD may be separate.");
    return Result;
}
