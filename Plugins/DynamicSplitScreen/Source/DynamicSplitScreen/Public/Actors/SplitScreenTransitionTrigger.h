// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplitScreenTransitionTrigger.generated.h"

class UBoxComponent;
class ACharacter;

/**
 * Trigger actor placed in a level. When the local player enters, this machine's screen transitions
 * from split screen to full screen. When they leave, it returns to split screen.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ASplitScreenTransitionTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASplitScreenTransitionTrigger();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** If true, reacts to the player this machine controls. If false, reacts to the local player with FixedFullScreenPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	bool bFullScreenForEnteringPlayer = true;

	/** Local player index (ControllerId) used if bFullScreenForEnteringPlayer is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen", meta = (EditCondition = "!bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/**
	 * Characters that made this machine go full screen -> number of their overlapping components.
	 * One character can overlap with several components (capsule, mesh ...), so Begin/End arrive several times.
	 */
	TMap<TWeakObjectPtr<ACharacter>, int32> OccupantOverlapCounts;
};
