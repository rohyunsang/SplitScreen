// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SSGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API USSGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    // 생성자
    USSGameInstance();

    // UGameInstance 오버라이드
    virtual void Init() override;
    virtual void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) override;

    // 스플릿 스크린 기본 기능
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void EnableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void DisableSplitScreen();

    // 상태 확인 함수들
    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsDualModeEnabled() const { return bUseDualMode; }

protected:
    // 스플릿 스크린 설정
    void SetupSplitScreenViewport();

    // 설정 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bUseDualMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    int32 MaxSplitScreenPlayers = 2;

    // 런타임 상태
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bSplitScreenActive = false;

    // 타이머 핸들들
    FTimerHandle SplitScreenTimerHandle;
    FTimerHandle DualModeToggleHandle;
};
