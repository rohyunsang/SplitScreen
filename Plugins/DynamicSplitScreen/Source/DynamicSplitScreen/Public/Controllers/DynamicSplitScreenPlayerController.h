// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DynamicSplitScreenPlayerController.generated.h"

class ADynamicSplitScreenSpectatorPawn;

/**
 * Base PlayerController for online split-screen.
 *
 * On the client, automatically creates a dummy local player + spectator pawn
 * that follows the remote character, enabling a second viewport for split-screen.
 *
 * Usage:
 *   1. Inherit your PlayerController from ADynamicSplitScreenPlayerController.
 *   2. Set DummySpectatorPawnClass to your own SpectatorPawn subclass if needed.
 *   3. Override GetRemoteCharacterCameraRotation() to feed your replicated
 *      camera rotation into the dummy pawn (for accurate view sync).
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADynamicSplitScreenPlayerController();

	/** True when this controller is the dummy local player (second viewport). */
	UPROPERTY(BlueprintReadOnly, Category = "Dynamic Split Screen")
	bool bIsDummyController = false;

	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void SetAsDummyController(bool bDummy);

	/** The SpectatorPawn class to spawn as the dummy pawn. Override to use your own subclass. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen")
	TSubclassOf<ADynamicSplitScreenSpectatorPawn> DummySpectatorPawnClass;

protected:
	virtual void BeginPlay() override;

	/**
	 * Override this to return your character's replicated camera rotation.
	 * Default returns the character's actor rotation (no replication).
	 * For accurate view sync, return your replicated FRotator here.
	 *
	 * Example:
	 *   if (AMyCharacter* MyChar = Cast<AMyCharacter>(RemoteChar))
	 *       return MyChar->GetReplicatedCameraRotation();
	 *   return Super::GetRemoteCharacterCameraRotation(RemoteChar);
	 */
	virtual FRotator GetRemoteCharacterCameraRotation(ACharacter* RemoteChar) const;

private:
	void SetupClientSplitScreen();

	UPROPERTY()
	bool bClientSplitScreenSetupComplete = false;

	FTimerHandle ClientSetupRetryHandle;
	FTimerHandle ClientSyncTimerHandle;

	UPROPERTY()
	ADynamicSplitScreenSpectatorPawn* ClientDummyPawn;

	void CreateClientDummyPawn();
	void AttachDummySpectatorToRemoteCharacter(ADynamicSplitScreenSpectatorPawn* DummyPawn);
	void SyncClientDummyWithRemotePlayer(ADynamicSplitScreenSpectatorPawn* DummyPawn);
	void StartClientDummySync(ADynamicSplitScreenSpectatorPawn* DummyPawn);

	TWeakObjectPtr<ACharacter> CachedRemoteCharacter;
};
