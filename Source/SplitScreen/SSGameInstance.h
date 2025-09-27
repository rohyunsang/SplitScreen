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
    // ������
    USSGameInstance();

    // UGameInstance �������̵�
    virtual void Init() override;
    virtual void OnWorldChanged(UWorld* OldWorld, UWorld* NewWorld) override;

    // ���ø� ��ũ�� �⺻ ���
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void EnableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void DisableSplitScreen();

    // ���� Ȯ�� �Լ���
    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsDualModeEnabled() const { return bUseDualMode; }

protected:
    // ���ø� ��ũ�� ����
    void SetupSplitScreenViewport();

    // ���� ������
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bUseDualMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    int32 MaxSplitScreenPlayers = 2;

    // ��Ÿ�� ����
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bSplitScreenActive = false;

    // Ÿ�̸� �ڵ��
    FTimerHandle SplitScreenTimerHandle;
    FTimerHandle DualModeToggleHandle;
};
