// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FixedCameraVolume.generated.h"

class UBoxComponent;
class UCameraComponent;

/**
 * Trigger actor that forces a fixed camera perspective on entering characters.
 * Restores the original 3rd-person camera upon exiting.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API AFixedCameraVolume : public AActor
{
	GENERATED_BODY()

public:
	AFixedCameraVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Camera Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Fixed camera component determining the perspective */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Camera Volume")
	TObjectPtr<UCameraComponent> FixedCamera;

	/** Camera blend duration (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	float BlendTime = 0.75f;

	/** Fixed control rotation forced on the player controller within the volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	FRotator FixedControlRotation = FRotator(0.f, 0.f, 0.f);

	/** If true, triggers a full screen transition when entered, and returns to split screen when exited */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	bool bUseSplitScreenTransition = false;

	/** If true, converts the viewport of the entering player to full screen. If false, uses FixedFullScreenPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume", meta = (EditCondition = "bUseSplitScreenTransition"))
	bool bFullScreenForEnteringPlayer = true;

	/** Fixed player index to use if bFullScreenForEnteringPlayer is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume", meta = (EditCondition = "bUseSplitScreenTransition && !bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/** Stored control rotations prior to entering the volume */
	TMap<APlayerController*, FRotator> SavedControlRotations;

	/** Number of players currently inside the trigger */
	int32 PlayersInTrigger = 0;
};
