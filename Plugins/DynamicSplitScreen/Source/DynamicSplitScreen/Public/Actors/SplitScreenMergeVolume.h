// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplitScreenMergeVolume.generated.h"

class UBoxComponent;
class ACharacter;

/**
 * Volume that merges the split screen into a single view when the required number of players
 * (local or remote) are inside, and restores split screen when they leave.
 * Each machine merges to its own player's view. Does not switch cameras — purely controls the layout.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ASplitScreenMergeVolume : public AActor
{
	GENERATED_BODY()

public:
	ASplitScreenMergeVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Split Screen Merge Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/**
	 * Number of players required inside the volume to trigger the merge.
	 * Set to 1 to merge as soon as any player enters.
	 * Set to 2 to merge only when both players are inside.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Split Screen Merge Volume", meta = (ClampMin = "1", ClampMax = "2"))
	int32 PlayersRequiredToMerge = 2;

	/**
	 * If true, restores split screen as soon as the player count drops below PlayersRequiredToMerge.
	 * If false, restores only when all players have left.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Split Screen Merge Volume")
	bool bRestoreOnAnyPlayerExit = true;

private:
	/** Player characters inside -> number of their overlapping components */
	TMap<TWeakObjectPtr<ACharacter>, int32> OccupantOverlapCounts;

	bool bCurrentlyMerged = false;

	int32 GetPlayersInside();
	void UpdateMergeState();
};
