// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SplitScreenPlayerController.generated.h"

UCLASS()
class SPLITSCREEN_API ASplitScreenPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASplitScreenPlayerController();

	UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
	bool bIsDummyController = false;

	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void SetAsDummyController(bool bDummy);

protected:
	virtual void BeginPlay() override;

private:
	void SetupClientSplitScreen();

	UPROPERTY()
	bool bClientSplitScreenSetupComplete = false;

	FTimerHandle ClientSetupRetryHandle;
	FTimerHandle ClientSyncTimerHandle;

	UPROPERTY()
	class ASplitScreenSpectatorPawn* ClientDummyPawn;

	void CreateClientDummyPawn();
	void AttachDummySpectatorToRemoteCharacter(class ASplitScreenSpectatorPawn* DummyPawn);
	void SyncClientDummyWithRemotePlayer(class ASplitScreenSpectatorPawn* DummyPawn);
	void StartClientDummySync(class ASplitScreenSpectatorPawn* DummyPawn);

	TWeakObjectPtr<class ASplitScreenCharacter> CachedRemoteCharacter;
};
