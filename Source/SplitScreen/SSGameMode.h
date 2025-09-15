// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSGameMode.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
    ASSGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<APawn> DummySpectatorPawnClass;

protected:
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetupOnlineSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void UpdateSplitScreenLayout();

private:
    TArray<APlayerController*> ConnectedPlayers;

    class ASSDummySpectatorPawn* DummySpectatorPawn;
    class ASSPlayerController* DummyPlayerController;

    void CreateDummyLocalPlayer();
    void SyncDummyPlayerWithProxy();  // 이름 변경: 프록시 사용
    void ApplyProxyCamera(class ASSDummySpectatorPawn* DummyPawn, const struct FRepCamInfo& CamData);
    void SetupCameraProxies();
    FTimerHandle SyncTimerHandle;

public:
    UPROPERTY() // GC 보호
        class ASSCameraViewProxy* ServerCamProxy = nullptr;

    UPROPERTY() // GC 보호  
        class ASSCameraViewProxy* ClientCamProxy = nullptr;

    // 캐시된 프록시 참조 (성능 최적화)
private:
    TWeakObjectPtr<class ASSCameraViewProxy> CachedClientProxy;
};
