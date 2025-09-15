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


    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void UpdateSplitScreenLayout();

private:
    TArray<APlayerController*> ConnectedPlayers;
    class ASSDummySpectatorPawn* DummySpectatorPawn;
    class ASSPlayerController* DummyPlayerController;


    FTimerHandle SyncTimerHandle;


public:
    UPROPERTY() // GC º¸È£
    class ASSCameraViewProxy* ServerCamProxy = nullptr;
};
