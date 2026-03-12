// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraZoomVolume.generated.h"

class UBoxComponent;

/**
 * Volume that smoothly adjusts the camera zoom (SpringArm length) when a character enters.
 * Optionally merges split screen into a single viewport when both players are inside.
 * Restores original zoom and split screen when players exit.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ACameraZoomVolume : public AActor
{
	GENERATED_BODY()

public:
	ACameraZoomVolume();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Zoom Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Target SpringArm length inside the volume. Smaller = zoom in, Larger = zoom out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Zoom Volume")
	float TargetArmLength = 200.f;

	/** How fast the zoom interpolates (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Zoom Volume")
	float ZoomInterpSpeed = 5.f;

	/** If true, merges split screen into one viewport when both players are inside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Zoom Volume|Split Screen")
	bool bMergeWhenBothInside = false;

	/** Which player's view to show when merged (0 = Player 1, 1 = Player 2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Zoom Volume|Split Screen", meta = (EditCondition = "bMergeWhenBothInside", ClampMin = "0", ClampMax = "1"))
	int32 MergedPlayerIndex = 0;

	/** If true, uses the entering player's index instead of MergedPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Zoom Volume|Split Screen", meta = (EditCondition = "bMergeWhenBothInside"))
	bool bMergeToEnteringPlayer = false;

private:
	struct FZoomState
	{
		float OriginalArmLength = 400.f;
		bool bInside = false;
	};

	TMap<ACharacter*, FZoomState> ZoomStates;

	int32 PlayersInside = 0;
	bool bCurrentlyMerged = false;

	void TryMergeSplitScreen(ACharacter* EnteringCharacter);
	void TryRestoreSplitScreen();
};
