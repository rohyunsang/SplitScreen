// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SplitScreenGameMode.generated.h"

// NOT USING 
// NOT USING 
// NOT USING 
// NOT USING 
// NOT USING 
// NOT USING 
// NOT USING 
// NOT USING 

UCLASS(minimalapi)
class ASplitScreenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASplitScreenGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void AttachDummySpectatorToClient(APlayerController* RemoteClient);
	void SyncDummyWithRemoteClient();

	UPROPERTY()
	class ASplitScreenSpectatorPawn* DummySpectatorPawn;

	UPROPERTY()
	APlayerController* DummyPlayerController;

	UPROPERTY()
	APlayerController* CachedRemoteClient;

	FTimerHandle SyncTimerHandle;
};



