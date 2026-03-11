// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplitScreenTransitionTrigger.generated.h"

class UBoxComponent;

/**
 * Trigger actor placed in a level. When a character enters, it triggers a transition 
 * from Split Screen to Full Screen. When leaving, Full Screen to Split Screen.
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

	/** If true, converts the viewport of the entering player to full screen. If false, uses FixedFullScreenPlayerIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	bool bFullScreenForEnteringPlayer = true;

	/** Fixed player index to use if bFullScreenForEnteringPlayer is false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen", meta = (EditCondition = "!bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/** Number of players currently inside the trigger */
	int32 PlayersInTrigger = 0;
};
