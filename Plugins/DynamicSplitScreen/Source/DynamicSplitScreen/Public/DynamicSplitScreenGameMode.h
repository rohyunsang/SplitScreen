// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DynamicSplitScreenGameMode.generated.h"

class ADynamicSplitScreenSpectatorPawn;
class ADynamicSplitScreenPlayerController;

/**
 * Base GameMode for host-side split-screen setup.
 *
 * On BeginPlay (host/listen server), creates a dummy local player + controller
 * so the host can display split-screen alongside the remote client.
 * On PostLogin, attaches a dummy SpectatorPawn to the remote client's pawn.
 *
 * Usage:
 *   1. Inherit your GameMode from ADynamicSplitScreenGameMode.
 *   2. Set DummySpectatorPawnClass / DummyPlayerControllerClass if using subclasses.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADynamicSplitScreenGameMode();

	/** SpectatorPawn class to spawn as the dummy for the second viewport. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen")
	TSubclassOf<ADynamicSplitScreenSpectatorPawn> DummySpectatorPawnClass;

	/** PlayerController class to spawn for the dummy local player. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen")
	TSubclassOf<ADynamicSplitScreenPlayerController> DummyPlayerControllerClass;

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void AttachDummySpectatorToClient(APlayerController* RemoteClient);
	void SyncDummyWithRemoteClient();

	UPROPERTY()
	ADynamicSplitScreenSpectatorPawn* DummySpectatorPawn;

	UPROPERTY()
	APlayerController* DummyPlayerController;

	UPROPERTY()
	APlayerController* CachedRemoteClient;

	FTimerHandle SyncTimerHandle;
};
