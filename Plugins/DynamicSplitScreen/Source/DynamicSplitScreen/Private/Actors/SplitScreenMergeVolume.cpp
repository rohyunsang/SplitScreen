// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/SplitScreenMergeVolume.h"
#include "DynamicSplitScreen.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"

ASplitScreenMergeVolume::ASplitScreenMergeVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASplitScreenMergeVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ASplitScreenMergeVolume::OnTriggerEndOverlap);
}

void ASplitScreenMergeVolume::BeginPlay()
{
	Super::BeginPlay();
	OccupantOverlapCounts.Empty();
	bCurrentlyMerged = false;
}

void ASplitScreenMergeVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);

	// Only player characters count (PlayerState is replicated, so remote players are counted on every machine)
	if (!Character || !Character->GetPlayerState()) return;

	int32& OverlapCount = OccupantOverlapCounts.FindOrAdd(Character);
	if (OverlapCount++ > 0)
	{
		return;
	}

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("SplitScreenMergeVolume: %s entered (%d/%d players inside)"),
		*Character->GetName(), GetPlayersInside(), PlayersRequiredToMerge);

	UpdateMergeState();
}

void ASplitScreenMergeVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	int32* OverlapCount = OccupantOverlapCounts.Find(Character);
	if (!OverlapCount) return;

	if (--(*OverlapCount) > 0)
	{
		return;
	}
	OccupantOverlapCounts.Remove(Character);

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("SplitScreenMergeVolume: %s exited (%d players inside)"),
		*Character->GetName(), GetPlayersInside());

	UpdateMergeState();
}

int32 ASplitScreenMergeVolume::GetPlayersInside()
{
	for (auto It = OccupantOverlapCounts.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	return OccupantOverlapCounts.Num();
}

void ASplitScreenMergeVolume::UpdateMergeState()
{
	UGameInstance* GI = GetGameInstance();
	UDynamicSplitScreenSubsystem* Subsystem = GI ? GI->GetSubsystem<UDynamicSplitScreenSubsystem>() : nullptr;
	if (!Subsystem) return;

	const int32 PlayersInside = GetPlayersInside();

	if (!bCurrentlyMerged)
	{
		if (PlayersInside >= PlayersRequiredToMerge)
		{
			Subsystem->RequestFullScreen(this);
			bCurrentlyMerged = true;
			UE_LOG(LogDynamicSplitScreen, Log, TEXT("SplitScreenMergeVolume: merged"));
		}
		return;
	}

	const bool bShouldRestore = bRestoreOnAnyPlayerExit
		? (PlayersInside < PlayersRequiredToMerge)
		: (PlayersInside <= 0);

	if (bShouldRestore)
	{
		Subsystem->ReleaseFullScreen(this);
		bCurrentlyMerged = false;
		UE_LOG(LogDynamicSplitScreen, Log, TEXT("SplitScreenMergeVolume: restored split screen"));
	}
}
