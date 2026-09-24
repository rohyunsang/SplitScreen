// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FixedCameraVolume.generated.h"

class UBoxComponent;
class UCameraComponent;
class ACharacter;
class APlayerController;

/**
 * Trigger actor that forces a fixed camera perspective on entering characters.
 * The camera is locked to this actor's CameraComponent. The control rotation is blended
 * so the movement direction changes smoothly. Restores the 3rd-person camera upon exiting.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API AFixedCameraVolume : public AActor
{
	GENERATED_BODY()

public:
	AFixedCameraVolume();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

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

	/** Control rotation used inside the volume (defines the movement direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	FRotator FixedControlRotation = FRotator(0.f, 0.f, 0.f);

	/** Control rotation blend duration (seconds). Independent of the camera BlendTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	float ControlRotationBlendTime = 0.75f;

	/**
	 * If false (default), keeps the current yaw on exit so the movement direction does not jump; only pitch/roll are restored.
	 * If true, fully restores the control rotation from before entering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	bool bRestoreControlRotationOnExit = false;

	/** If true, triggers a full screen transition when entered, and returns to split screen when exited */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	bool bUseSplitScreenTransition = false;

	/** If true, reacts to the player this machine controls. If false, reacts to the local player with FixedFullScreenPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume", meta = (EditCondition = "bUseSplitScreenTransition"))
	bool bFullScreenForEnteringPlayer = true;

	/** Local player index (ControllerId) used if bFullScreenForEnteringPlayer is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume", meta = (EditCondition = "bUseSplitScreenTransition && !bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/**
	 * Per-character state. Overlaps are counted because one character can overlap with several components,
	 * and SetIgnoreLookInput is a stack counter: mismatched lock/unlock would lock look input forever.
	 */
	struct FOccupant
	{
		TWeakObjectPtr<APlayerController> LockedPC;
		FRotator SavedControlRotation = FRotator::ZeroRotator;
		int32 OverlapCount = 0;
		bool bLockedInput = false;
		bool bRequestedFullScreen = false;
	};

	TMap<TWeakObjectPtr<ACharacter>, FOccupant> Occupants;

	struct FControlRotationBlend
	{
		FRotator StartRotation = FRotator::ZeroRotator;
		FRotator TargetRotation = FRotator::ZeroRotator;
		float Elapsed = 0.f;
		float Duration = 0.75f;
	};

	TMap<TWeakObjectPtr<APlayerController>, FControlRotationBlend> ControlRotationBlends;

	void StartControlRotationBlend(APlayerController* PC, const FRotator& Target);
};
