// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSGameMode.generated.h"

class ASSPlayerController;

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSGameMode : public AGameModeBase
{
	GENERATED_BODY()
	

public:
    ASSGameMode();

protected:
    // ���� �� ��Ʈ�ѷ���(���������� ����)
    UPROPERTY()
    TArray<TObjectPtr<ASSPlayerController>> ConnectedPlayers;

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

private:
    static bool IsPaired(const ASSPlayerController* PC);
    void ClearPartner(ASSPlayerController* PC);
    void TryAutoPair();                 // �ٽ�: �� �� ���� ��� ��� ��Ī
};
