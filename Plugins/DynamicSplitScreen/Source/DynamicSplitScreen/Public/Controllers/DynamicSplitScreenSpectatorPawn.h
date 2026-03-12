// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "DynamicSplitScreenSpectatorPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * Invisible spectator pawn used as a dummy local player for online split-screen.
 * Attach this to a remote character to provide a second viewport.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	ADynamicSplitScreenSpectatorPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

protected:
	virtual void BeginPlay() override;
};
