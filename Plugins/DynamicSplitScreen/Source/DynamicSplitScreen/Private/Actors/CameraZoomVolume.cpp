// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/CameraZoomVolume.h"
#include "DynamicSplitScreen.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
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
	ZoomStates.Empty();
}

void ACameraZoomVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (auto It = ZoomStates.CreateIterator(); It; ++It)
	{
		ACharacter* Character = It->Key.Get();
		const FZoomState& State = It->Value;

		USpringArmComponent* SpringArm = IsValid(Character) ? Character->FindComponentByClass<USpringArmComponent>() : nullptr;
		if (!SpringArm)
		{
			It.RemoveCurrent();
			continue;
		}

		const bool bInside = State.OverlapCount > 0;
		const float Target = bInside ? TargetArmLength : State.OriginalArmLength;
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, Target, DeltaSeconds, ZoomInterpSpeed);

		// Finished restoring after exit
		if (!bInside && FMath::IsNearlyEqual(SpringArm->TargetArmLength, State.OriginalArmLength, 1.f))
		{
			SpringArm->TargetArmLength = State.OriginalArmLength;
			It.RemoveCurrent();
		}
	}
}

void ACameraZoomVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsLocallyControlled()) return;

	USpringArmComponent* SpringArm = Character->FindComponentByClass<USpringArmComponent>();
	if (!SpringArm) return;

	// A character still blending back keeps its original length; only a fresh entry captures it
	FZoomState* Existing = ZoomStates.Find(Character);
	FZoomState& State = Existing ? *Existing : ZoomStates.Add(Character);
	if (!Existing)
	{
		State.OriginalArmLength = SpringArm->TargetArmLength;
	}

	if (State.OverlapCount++ > 0)
	{
		return;
	}

	if (bUseSplitScreenTransition
		&& UDynamicSplitScreenSubsystem::ShouldLocalViewRespondTo(Character, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		State.bRequestedFullScreen = true;

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->RequestFullScreen(this);
			}
		}
	}

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("CameraZoomVolume: %s entered -> Zoom to %.0f"), *Character->GetName(), TargetArmLength);
}

void ACameraZoomVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	FZoomState* State = ZoomStates.Find(Character);
	if (!State || State->OverlapCount <= 0) return;

	if (--State->OverlapCount > 0)
	{
		return;
	}

	// Keep the entry so Tick can blend the arm back; just drop the full screen request
	const bool bWasFullScreenRequester = State->bRequestedFullScreen;
	State->bRequestedFullScreen = false;

	bool bAnyRequesterLeft = false;
	for (const auto& Pair : ZoomStates)
	{
		bAnyRequesterLeft |= Pair.Key.IsValid() && Pair.Value.bRequestedFullScreen;
	}

	if (bUseSplitScreenTransition && bWasFullScreenRequester && !bAnyRequesterLeft)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->ReleaseFullScreen(this);
			}
		}
	}

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("CameraZoomVolume: %s exited -> Restore zoom"), *Character->GetName());
}
