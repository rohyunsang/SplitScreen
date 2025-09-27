// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "SSGameMode.generated.h"

struct FCameraPredictionData;

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

    UPROPERTY() // GC º¸È£
    class ASSCameraViewProxy* ServerCamProxy = nullptr;

    UPROPERTY()
    TMap<APlayerController*, ASSCameraViewProxy*> ClientCamProxies;

private:
    TArray<APlayerController*> ConnectedPlayers;
    class ASSDummySpectatorPawn* DummySpectatorPawn;
    class ASSPlayerController* DummyPlayerController;

    void CreateDummyLocalPlayer();
    void AttachDummySpectatorToClient(APlayerController* RemoteClient);
    void SyncDummyRotationWithProxy();
    void SetupOnlineSplitScreen();

    FTimerHandle SyncTimerHandle;
    FTimerHandle RotationSyncTimerHandle;
};
