// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/CameraZoomVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"

ACameraZoomVolume::ACameraZoomVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACameraZoomVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACameraZoomVolume::OnTriggerEndOverlap);
}

void ACameraZoomVolume::BeginPlay()
{
	Super::BeginPlay();
	PlayersInside = 0;
	bCurrentlyMerged = false;
}

void ACameraZoomVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (auto& Pair : ZoomStates)
	{
		ACharacter* Character = Pair.Key;
		FZoomState& State = Pair.Value;

		if (!IsValid(Character)) continue;

		USpringArmComponent* SpringArm = Character->FindComponentByClass<USpringArmComponent>();
		if (!SpringArm) continue;

		const float Target = State.bInside ? TargetArmLength : State.OriginalArmLength;
		SpringArm->TargetArmLength = FMath::FInterpTo(
			SpringArm->TargetArmLength,
			Target,
			DeltaSeconds,
			ZoomInterpSpeed
		);
	}

	// Clean up characters outside the volume that have finished restoring
	for (auto It = ZoomStates.CreateIterator(); It; ++It)
	{
		ACharacter* Character = It->Key;
		FZoomState& State = It->Value;
		if (!State.bInside)
		{
			if (!IsValid(Character)) { It.RemoveCurrent(); continue; }
			USpringArmComponent* SpringArm = Character->FindComponentByClass<USpringArmComponent>();
			if (SpringArm && FMath::IsNearlyEqual(SpringArm->TargetArmLength, State.OriginalArmLength, 1.f))
			{
				SpringArm->TargetArmLength = State.OriginalArmLength;
				It.RemoveCurrent();
			}
		}
	}
}

void ACameraZoomVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	if (!Character->IsLocallyControlled()) return;

	USpringArmComponent* SpringArm = Character->FindComponentByClass<USpringArmComponent>();
	if (!SpringArm) return;

	FZoomState& State = ZoomStates.FindOrAdd(Character);
	if (!State.bInside)
	{
		State.OriginalArmLength = SpringArm->TargetArmLength;
		State.bInside = true;
		PlayersInside++;
	}

	if (bMergeOnEnter)
	{
		TryMergeSplitScreen(Character);
	}

	UE_LOG(LogTemp, Log, TEXT("CameraZoomVolume: %s entered -> Zoom to %.0f"), *Character->GetName(), TargetArmLength);
}

void ACameraZoomVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	if (!Character->IsLocallyControlled()) return;

	FZoomState* State = ZoomStates.Find(Character);
	if (State && State->bInside)
	{
		State->bInside = false;
		PlayersInside = FMath::Max(0, PlayersInside - 1);
	}

	if (bMergeOnEnter)
	{
		TryRestoreSplitScreen();
	}

	UE_LOG(LogTemp, Log, TEXT("CameraZoomVolume: %s exited -> Restore zoom"), *Character->GetName());
}

void ACameraZoomVolume::TryMergeSplitScreen(ACharacter* EnteringCharacter)
{
	const int32 RequiredCount = bRequireBothPlayers ? 2 : 1;
	if (!bCurrentlyMerged && PlayersInside >= RequiredCount)
	{
		UGameInstance* GI = GetGameInstance();
		if (!GI) return;

		UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>();
		if (!Subsystem) return;

		int32 TargetIndex = MergedPlayerIndex;
		if (bMergeToEnteringPlayer)
		{
			if (APlayerController* PC = Cast<APlayerController>(EnteringCharacter->GetController()))
			{
				if (ULocalPlayer* LP = PC->GetLocalPlayer())
				{
					TargetIndex = LP->GetControllerId();
				}
			}
		}

		Subsystem->TransitionToFullScreen(TargetIndex);
		bCurrentlyMerged = true;
		UE_LOG(LogTemp, Log, TEXT("CameraZoomVolume: Both players inside -> Merge to player %d"), TargetIndex);
	}
}

void ACameraZoomVolume::TryRestoreSplitScreen()
{
	const int32 RequiredCount = bRequireBothPlayers ? 2 : 1;
	if (bCurrentlyMerged && PlayersInside < RequiredCount)
	{
		UGameInstance* GI = GetGameInstance();
		if (!GI) return;

		UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>();
		if (!Subsystem) return;

		Subsystem->TransitionToSplitScreen();
		bCurrentlyMerged = false;
		UE_LOG(LogTemp, Log, TEXT("CameraZoomVolume: Player left -> Restore split screen"));
	}
}
