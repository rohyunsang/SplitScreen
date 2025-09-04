// Fill out your copyright notice in the Description page of Project Settings.


#include "SSGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"

USSGameInstance::USSGameInstance()
{
    // 기본값 설정
    bEnableSplitScreen = true;
    bUseDualMode = true;
    MaxSplitScreenPlayers = 2;
    bSplitScreenActive = false;
}

void USSGameInstance::Init()
{
    Super::Init();

    UE_LOG(LogTemp, Warning, TEXT("SS GameInstance Initialized"));

    if (bEnableSplitScreen)
    {
        SetupSplitScreenViewport();
    }
}

void USSGameInstance::OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld)
{
    Super::OnWorldChanged(OldWorld, NewWorld);

    UE_LOG(LogTemp, Warning, TEXT("SS World Changed - Old: %s, New: %s"),
        OldWorld ? *OldWorld->GetName() : TEXT("None"),
        NewWorld ? *NewWorld->GetName() : TEXT("None"));

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
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Cannot setup viewport - GEngine or GameViewport is null"));
        return;
    }

    // 스플릿 스크린을 항상 활성화
    GEngine->GameViewport->SetForceDisableSplitscreen(false);
    GEngine->GameViewport->MaxSplitscreenPlayers = MaxSplitScreenPlayers;

    // 듀얼 모드 설정
    if (bUseDualMode)
    {
        // 2인 플레이어용 가로 분할 설정
        GEngine->GameViewport->MaxSplitscreenPlayers = 2;
        UE_LOG(LogTemp, Warning, TEXT("SS Dual Mode Viewport Setup - Max Players: %d"), MaxSplitScreenPlayers);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Standard Viewport Setup - Max Players: %d"), MaxSplitScreenPlayers);
    }
}

void USSGameInstance::EnableSplitScreen()
{
    if (bSplitScreenActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split Screen already enabled"));
        return;
    }

    SetupSplitScreenViewport();
    bSplitScreenActive = true;

    FString ModeString = bUseDualMode ? TEXT("Dual Mode") : TEXT("Standard Mode");
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Enabled - %s"), *ModeString);
}

void USSGameInstance::DisableSplitScreen()
{
    if (!bSplitScreenActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split Screen already disabled"));
        return;
    }

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(true);
    }

    bSplitScreenActive = false;
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Disabled"));
}

void USSGameInstance::ToggleSplitScreen()
{
    if (bSplitScreenActive)
    {
        DisableSplitScreen();
    }
    else
    {
        EnableSplitScreen();
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Toggled - Now %s"),
        bSplitScreenActive ? TEXT("Enabled") : TEXT("Disabled"));
}

void USSGameInstance::SetDualMode(bool bEnabled)
{
    if (bUseDualMode == bEnabled)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Dual Mode already set to %s"),
            bEnabled ? TEXT("Enabled") : TEXT("Disabled"));
        return;
    }

    bUseDualMode = bEnabled;

    // 현재 스플릿 스크린이 활성화되어 있으면 재설정
    if (bSplitScreenActive)
    {
        // 일시적으로 비활성화 후 재활성화하여 새 설정 적용
        bool bWasActive = bSplitScreenActive;
        DisableSplitScreen();

        if (bWasActive)
        {
            // 약간의 지연 후 재활성화
            if (GetWorld())
            {
                GetWorld()->GetTimerManager().SetTimer(
                    DualModeToggleHandle,
                    [this]() { EnableSplitScreen(); },
                    0.1f,
                    false
                );
            }
        }
    }

    FString ModeString = bUseDualMode ? TEXT("Dual Mode") : TEXT("Standard Mode");
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Mode Changed to: %s"), *ModeString);
}

void USSGameInstance::ToggleDualMode()
{
    SetDualMode(!bUseDualMode);

    FString ModeString = bUseDualMode ? TEXT("Dual Mode") : TEXT("Standard Mode");
    UE_LOG(LogTemp, Warning, TEXT("SS Dual Mode Toggled to: %s"), *ModeString);
}