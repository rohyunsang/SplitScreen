// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/SplitScreenMergeVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
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
	PlayersInside = 0;
	bCurrentlyMerged = false;
}

void ASplitScreenMergeVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled()) return;

	PlayersInside++;

	UE_LOG(LogTemp, Log, TEXT("SplitScreenMergeVolume: %s entered (%d/%d players inside)"),
		*Character->GetName(), PlayersInside, PlayersRequiredToMerge);

	TryMerge(Character);
}

void ASplitScreenMergeVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled()) return;

	PlayersInside = FMath::Max(0, PlayersInside - 1);

	UE_LOG(LogTemp, Log, TEXT("SplitScreenMergeVolume: %s exited (%d players inside)"),
		*Character->GetName(), PlayersInside);

	TryRestore();
}

void ASplitScreenMergeVolume::TryMerge(ACharacter* EnteringCharacter)
{
	if (bCurrentlyMerged || PlayersInside < PlayersRequiredToMerge) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>();
	if (!Subsystem) return;

	ActiveMergedPlayerIndex = MergedPlayerIndex;

	if (bMergeToEnteringPlayer)
	{
		if (APlayerController* PC = Cast<APlayerController>(EnteringCharacter->GetController()))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				ActiveMergedPlayerIndex = LP->GetControllerId();
			}
		}
	}

	Subsystem->TransitionToFullScreen(ActiveMergedPlayerIndex);
	bCurrentlyMerged = true;

	UE_LOG(LogTemp, Log, TEXT("SplitScreenMergeVolume: Merged -> Player %d full screen"), ActiveMergedPlayerIndex);
}

void ASplitScreenMergeVolume::TryRestore()
{
	if (!bCurrentlyMerged) return;

	const bool bShouldRestore = bRestoreOnAnyPlayerExit
		? (PlayersInside < PlayersRequiredToMerge)
		: (PlayersInside <= 0);

	if (!bShouldRestore) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>();
	if (!Subsystem) return;

	Subsystem->TransitionToSplitScreen();
	bCurrentlyMerged = false;

	UE_LOG(LogTemp, Log, TEXT("SplitScreenMergeVolume: Restored split screen"));
}
