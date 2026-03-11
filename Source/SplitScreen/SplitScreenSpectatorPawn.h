// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "SplitScreenSpectatorPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class SPLITSCREEN_API ASplitScreenSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	ASplitScreenSpectatorPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

protected:
	virtual void BeginPlay() override;
};
