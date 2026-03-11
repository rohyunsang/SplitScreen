// Copyright Epic Games, Inc. All Rights Reserved.


#include "Actors/SplitScreenTransitionTrigger.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
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
	PlayersInTrigger = 0;
}

void ASplitScreenTransitionTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger++;

	// Determine target player index
	int32 TargetPlayerIndex = FixedFullScreenPlayerIndex;

	if (bFullScreenForEnteringPlayer)
	{
		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (LP)
		{
			TargetPlayerIndex = LP->GetControllerId();
		}
	}

	// Request transition via Subsystem
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
		{
			Subsystem->TransitionToFullScreen(TargetPlayerIndex);
			UE_LOG(LogTemp, Log, TEXT("SplitScreenTrigger: Player %d entered -> Full Screen transition"), TargetPlayerIndex);
		}
	}
}

void ASplitScreenTransitionTrigger::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	// Restore split screen when all players leave the trigger
	if (PlayersInTrigger <= 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->TransitionToSplitScreen();
				UE_LOG(LogTemp, Log, TEXT("SplitScreenTrigger: All players left -> Split Screen transition"));
			}
		}
	}
}
