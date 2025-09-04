// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SSGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API USSGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    virtual void Init() override;
    virtual void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    int32 MaxSplitScreenPlayers = 2;

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void EnableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void DisableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

private:
    bool bSplitScreenActive = false;
    void SetupSplitScreenViewport();

private:
    FTimerHandle SplitScreenTimerHandle;
};
