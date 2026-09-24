// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DynamicSplitScreenPlayerController.generated.h"

/**
 * Base PlayerController for online split screen.
 *
 * On a network client, enables split screen for the local player on BeginPlay.
 * (The host is handled by ADynamicSplitScreenGameMode.)
 * The other player's view is rendered by UDynamicSplitScreenViewportClient — no dummy player is created.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** If true, split screen is enabled automatically when this client's local controller begins play. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen")
	bool bAutoEnableSplitScreen = true;
};
