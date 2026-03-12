// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplitScreenMergeVolume.generated.h"

class UBoxComponent;
class ACharacter;

/**
 * Volume that merges the split screen into a single viewport when the required number
 * of players are inside, and restores split screen when they leave.
 * Does not switch cameras — purely controls viewport layout.
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
	 * If true, uses the first entering player's viewport index when merging.
	 * If false, uses MergedPlayerIndex.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Split Screen Merge Volume")
	bool bMergeToEnteringPlayer = false;

	/** Fixed player index to use when bMergeToEnteringPlayer is false (0 = Player 1, 1 = Player 2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Split Screen Merge Volume", meta = (EditCondition = "!bMergeToEnteringPlayer", ClampMin = "0", ClampMax = "1"))
	int32 MergedPlayerIndex = 0;

	/**
	 * If true, restores split screen as soon as any player leaves.
	 * If false, restores only when all players have left.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Split Screen Merge Volume")
	bool bRestoreOnAnyPlayerExit = true;

private:
	int32 PlayersInside = 0;
	bool bCurrentlyMerged = false;

	/** Player index stored when merging, used for log consistency. */
	int32 ActiveMergedPlayerIndex = 0;

	void TryMerge(ACharacter* EnteringCharacter);
	void TryRestore();
};
