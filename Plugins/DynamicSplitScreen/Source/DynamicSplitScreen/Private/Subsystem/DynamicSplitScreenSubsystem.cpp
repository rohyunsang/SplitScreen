// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "UI/DynamicSplitScreenViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

UDynamicSplitScreenSubsystem::UDynamicSplitScreenSubsystem()
{
    // Defaults
    bEnableSplitScreen = true;
    bUseDualMode = true;
    MaxSplitScreenPlayers = 2;
    bSplitScreenActive = false;
}

void UDynamicSplitScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (bEnableSplitScreen)
    {
        SetupSplitScreenViewport();
    }
}

void UDynamicSplitScreenSubsystem::SetupSplitScreenViewport()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogTemp, Error, TEXT("DynamicSplitScreen Cannot setup viewport - GEngine or GameViewport is null"));
        return;
    }

    // Always enable split screen
    GEngine->GameViewport->SetForceDisableSplitscreen(false);
    GEngine->GameViewport->MaxSplitscreenPlayers = MaxSplitScreenPlayers;

    // Dual Mode Setup
    if (bUseDualMode)
    {
        GEngine->GameViewport->MaxSplitscreenPlayers = 2;
        UE_LOG(LogTemp, Log, TEXT("Dual Mode Viewport Setup - Max Players: 2"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Standard Viewport Setup - Max Players: %d"), MaxSplitScreenPlayers);
    }
}

void UDynamicSplitScreenSubsystem::EnableSplitScreen()
{
    if (bSplitScreenActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("DynamicSplitScreen: Split Screen already enabled"));
        return;
    }

    SetupSplitScreenViewport();
    bSplitScreenActive = true;

    FString ModeString = bUseDualMode ? TEXT("Dual Mode") : TEXT("Standard Mode");
    UE_LOG(LogTemp, Warning, TEXT("DynamicSplitScreen Enabled - %s"), *ModeString);
}

void UDynamicSplitScreenSubsystem::DisableSplitScreen()
{
    if (!bSplitScreenActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("DynamicSplitScreen: Split Screen already disabled"));
        return;
    }

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(true);
    }

    bSplitScreenActive = false;
    UE_LOG(LogTemp, Warning, TEXT("DynamicSplitScreen Disabled"));
}

void UDynamicSplitScreenSubsystem::TransitionToFullScreen(int32 PlayerIndex)
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogTemp, Warning, TEXT("TransitionToFullScreen: GameViewport is null"));
        return;
    }

    UDynamicSplitScreenViewportClient* ViewportClient = Cast<UDynamicSplitScreenViewportClient>(GEngine->GameViewport);
    if (!ViewportClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("TransitionToFullScreen: Not using DynamicSplitScreenViewportClient"));
        return;
    }

    ViewportClient->StartFullScreenTransition(PlayerIndex);
    UE_LOG(LogTemp, Log, TEXT("TransitionToFullScreen: Requested for Player %d"), PlayerIndex);
}

void UDynamicSplitScreenSubsystem::TransitionToSplitScreen()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogTemp, Warning, TEXT("TransitionToSplitScreen: GameViewport is null"));
        return;
    }

    UDynamicSplitScreenViewportClient* ViewportClient = Cast<UDynamicSplitScreenViewportClient>(GEngine->GameViewport);
    if (!ViewportClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("TransitionToSplitScreen: Not using DynamicSplitScreenViewportClient"));
        return;
    }

    ViewportClient->StartSplitScreenTransition();
    UE_LOG(LogTemp, Log, TEXT("TransitionToSplitScreen: Requested"));
}

bool UDynamicSplitScreenSubsystem::IsInFullScreenMode() const
{
    if (!GEngine || !GEngine->GameViewport)
    {
        return false;
    }

    UDynamicSplitScreenViewportClient* ViewportClient = Cast<UDynamicSplitScreenViewportClient>(GEngine->GameViewport);
    if (!ViewportClient)
    {
        return false;
    }

    return ViewportClient->IsInFullScreenMode();
}
