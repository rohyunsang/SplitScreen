// Fill out your copyright notice in the Description page of Project Settings.


#include "SSGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

void USSGameInstance::Init()
{
    Super::Init();

    if (bEnableSplitScreen)
    {
        SetupSplitScreenViewport();
    }
}

void USSGameInstance::OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
    Super::OnWorldChanged(OldWorld, NewWorld);

    // 레벨 전환시에도 스플릿 스크린 유지
    if (bSplitScreenActive && NewWorld)
    {
        NewWorld->GetTimerManager().SetTimer(
            SplitScreenTimerHandle,
            FTimerDelegate::CreateLambda([this]()
                {
                    EnableSplitScreen();
                }),
            1.0f,
            false
        );
    }
}

void USSGameInstance::SetupSplitScreenViewport()
{
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(false);
        GEngine->GameViewport->MaxSplitscreenPlayers = MaxSplitScreenPlayers;
    }
}

void USSGameInstance::EnableSplitScreen()
{
    if (bSplitScreenActive) return;

    SetupSplitScreenViewport();
    bSplitScreenActive = true;

    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Enabled"));
}

void USSGameInstance::DisableSplitScreen()
{
    if (!bSplitScreenActive) return;

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(true);
    }

    bSplitScreenActive = false;
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Disabled"));
}