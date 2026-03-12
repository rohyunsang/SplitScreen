// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DynamicSplitScreenCharacter.generated.h"

/**
 * Base Character for Dynamic Split Screen.
 *
 * Replicates the camera rotation to all clients so the dummy spectator pawn
 * can accurately mirror the remote player's view.
 *
 * Usage:
 *   Inherit your character from ADynamicSplitScreenCharacter.
 *   No additional setup required.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADynamicSplitScreenCharacter();

	UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
	FRotator GetReplicatedCameraRotation() const { return ReplicatedCameraRotation; }

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	FRotator ReplicatedCameraRotation;
};
