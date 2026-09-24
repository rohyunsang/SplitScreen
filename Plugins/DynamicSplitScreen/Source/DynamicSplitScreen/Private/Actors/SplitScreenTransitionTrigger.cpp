// Copyright Epic Games, Inc. All Rights Reserved.


#include "Actors/SplitScreenTransitionTrigger.h"
#include "DynamicSplitScreen.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/GameInstance.h"

ASplitScreenTransitionTrigger::ASplitScreenTransitionTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASplitScreenTransitionTrigger::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ASplitScreenTransitionTrigger::OnTriggerEndOverlap);
}

void ASplitScreenTransitionTrigger::BeginPlay()
{
	Super::BeginPlay();
	OccupantOverlapCounts.Empty();
}

void ASplitScreenTransitionTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	// Transitions only change this machine's view
	if (!UDynamicSplitScreenSubsystem::ShouldLocalViewRespondTo(Character, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		return;
	}

	int32& OverlapCount = OccupantOverlapCounts.FindOrAdd(Character);
	if (OverlapCount++ > 0)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
		{
			Subsystem->RequestFullScreen(this);
			UE_LOG(LogDynamicSplitScreen, Log, TEXT("SplitScreenTrigger: %s entered -> Full Screen transition"), *Character->GetName());
		}
	}
}

void ASplitScreenTransitionTrigger::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	int32* OverlapCount = OccupantOverlapCounts.Find(Character);
	if (!OverlapCount)
	{
		return;	// this character did not change our screen
	}

	// Another component of the same character is still inside
	if (--(*OverlapCount) > 0)
	{
		return;
	}
	OccupantOverlapCounts.Remove(Character);

	// Weak keys of destroyed characters are not removed by Remove(nullptr)
	for (auto It = OccupantOverlapCounts.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (OccupantOverlapCounts.Num() == 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->ReleaseFullScreen(this);
				UE_LOG(LogDynamicSplitScreen, Log, TEXT("SplitScreenTrigger: all players left -> Split Screen transition"));
			}
		}
	}
}
