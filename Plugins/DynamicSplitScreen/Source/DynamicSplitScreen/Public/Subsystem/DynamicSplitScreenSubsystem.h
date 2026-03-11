// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/TimerHandle.h"
#include "DynamicSplitScreenSubsystem.generated.h"

/**
 * Subsystem to manage split-screen and full-screen states.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API UDynamicSplitScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
    UDynamicSplitScreenSubsystem();

    // UGameInstance Subsystem overrides
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Split Screen basic controls
    UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
    void EnableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
    void DisableSplitScreen();

    // Full screen animation transition
    UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
    void TransitionToFullScreen(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
    void TransitionToSplitScreen();

    // Status checks
    UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
    bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

    UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
    bool IsDualModeEnabled() const { return bUseDualMode; }

    UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
    bool IsInFullScreenMode() const;

protected:
    // Setup Viewport
    void SetupSplitScreenViewport();

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
    bool bEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
    bool bUseDualMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
    int32 MaxSplitScreenPlayers = 2;

    // Runtime state
    UPROPERTY(BlueprintReadOnly, Category = "Dynamic Split Screen")
    bool bSplitScreenActive = false;

    // Timer Handles
    FTimerHandle SplitScreenTimerHandle;
    FTimerHandle DualModeToggleHandle;
};
