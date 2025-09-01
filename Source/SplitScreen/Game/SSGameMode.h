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
    // 접속 중 컨트롤러들(서버에서만 유지)
    UPROPERTY()
    TArray<TObjectPtr<ASSPlayerController>> ConnectedPlayers;

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

private:
    static bool IsPaired(const ASSPlayerController* PC);
    void ClearPartner(ASSPlayerController* PC);
    void TryAutoPair();                 // 핵심: 두 명 비인 페어 즉시 매칭
};
