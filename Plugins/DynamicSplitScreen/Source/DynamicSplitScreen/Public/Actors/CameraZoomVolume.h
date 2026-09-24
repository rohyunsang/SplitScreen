// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraZoomVolume.generated.h"

class UBoxComponent;
class ACharacter;

/**
 * Volume that smoothly adjusts the camera zoom (SpringArm length) of the local player inside it.
 * Optionally switches this machine's screen to full screen while the local player is inside.
 * Restores the original zoom and split screen on exit.
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Camera Zoom Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Target SpringArm length inside the volume. Smaller = zoom in, Larger = zoom out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Zoom Volume")
	float TargetArmLength = 200.f;

	/** How fast the zoom interpolates (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Zoom Volume")
	float ZoomInterpSpeed = 5.f;

	/** If true, this machine's screen goes full screen while the local player is inside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Zoom Volume|Split Screen")
	bool bUseSplitScreenTransition = false;

	/** If true, reacts to the player this machine controls. If false, reacts to the local player with FixedFullScreenPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Zoom Volume|Split Screen", meta = (EditCondition = "bUseSplitScreenTransition"))
	bool bFullScreenForEnteringPlayer = true;

	/** Local player index (ControllerId) used if bFullScreenForEnteringPlayer is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Zoom Volume|Split Screen", meta = (EditCondition = "bUseSplitScreenTransition && !bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	struct FZoomState
	{
		float OriginalArmLength = 400.f;
		int32 OverlapCount = 0;
		bool bRequestedFullScreen = false;
	};

	/** Characters inside the volume, and characters still blending back (OverlapCount == 0) */
	TMap<TWeakObjectPtr<ACharacter>, FZoomState> ZoomStates;
};
