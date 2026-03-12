// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FollowCameraVolume.generated.h"

class UBoxComponent;
class UCameraComponent;
class ACharacter;
class AFollowCameraActor;

/**
 * Internal actor spawned per player when entering a FollowCameraVolume.
 * Follows the target character each tick at a fixed offset and rotation.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class DYNAMICSPLITSCREEN_API AFollowCameraActor : public AActor
{
	GENERATED_BODY()

public:
	AFollowCameraActor();

	virtual void Tick(float DeltaSeconds) override;

	void InitFollow(ACharacter* InTarget, const FVector& InOffset, const FRotator& InRotation, bool bInFollowCharacterYaw);

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY()
	TObjectPtr<ACharacter> TargetCharacter;

	FVector FollowOffset;
	FRotator FixedRotation;
	bool bFollowCharacterYaw = false;
};

// ─────────────────────────────────────────────────────────────

/**
 * Volume that switches the player to a follow camera when entered.
 * The camera maintains a fixed offset and rotation relative to the character,
 * following its position while ignoring player look input.
 * Restores the original camera on exit.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API AFollowCameraVolume : public AActor
{
	GENERATED_BODY()

public:
	AFollowCameraVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Follow Camera Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Offset from the character's location in world space (or local space if bFollowCharacterYaw is true). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume")
	FVector FollowOffset = FVector(-500.f, 0.f, 300.f);

	/** Fixed rotation applied to the follow camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume")
	FRotator FixedCameraRotation = FRotator(-20.f, 0.f, 0.f);

	/**
	 * If true, the offset rotates with the character's yaw
	 * so the camera always stays behind the character.
	 * If false, the offset is in world space (e.g. fixed side-scroller angle).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume")
	bool bFollowCharacterYaw = false;

	/** Camera blend duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume")
	float BlendTime = 0.75f;

	/** If true, locks player look input while inside the volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume")
	bool bIgnoreLookInput = true;

	/** If true, triggers a split screen transition when a player enters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume|Split Screen")
	bool bUseSplitScreenTransition = false;

	/** If true, the entering player's viewport goes full screen. If false, uses FixedFullScreenPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume|Split Screen", meta = (EditCondition = "bUseSplitScreenTransition"))
	bool bFullScreenForEnteringPlayer = true;

	/** Fixed player index to use when bFullScreenForEnteringPlayer is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Follow Camera Volume|Split Screen", meta = (EditCondition = "bUseSplitScreenTransition && !bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/** Spawned follow camera actors, keyed by player controller. */
	UPROPERTY()
	TMap<TObjectPtr<APlayerController>, TObjectPtr<AFollowCameraActor>> SpawnedCameras;

	int32 PlayersInTrigger = 0;
};
