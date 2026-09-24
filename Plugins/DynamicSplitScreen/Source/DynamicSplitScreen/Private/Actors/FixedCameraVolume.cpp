// Copyright Epic Games, Inc. All Rights Reserved.


#include "Actors/FixedCameraVolume.h"
#include "DynamicSplitScreen.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/GameInstance.h"

AFixedCameraVolume::AFixedCameraVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AFixedCameraVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AFixedCameraVolume::OnTriggerEndOverlap);

	// Editor-adjustable fixed camera perspective
	FixedCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FixedCamera"));
	FixedCamera->SetupAttachment(RootComponent);
	FixedCamera->SetRelativeLocation(FVector(0.f, -800.f, 200.f));
	FixedCamera->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
}

void AFixedCameraVolume::BeginPlay()
{
	Super::BeginPlay();
	Occupants.Empty();
	ControlRotationBlends.Empty();
}

void AFixedCameraVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (auto It = ControlRotationBlends.CreateIterator(); It; ++It)
	{
		APlayerController* PC = It->Key.Get();
		FControlRotationBlend& Blend = It->Value;

		if (!IsValid(PC))
		{
			It.RemoveCurrent();
			continue;
		}

		Blend.Elapsed += DeltaTime;
		const float Alpha = FMath::Clamp(Blend.Elapsed / FMath::Max(Blend.Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float SmoothedAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

		PC->SetControlRotation(FMath::Lerp(Blend.StartRotation, Blend.TargetRotation, SmoothedAlpha));

		if (Alpha >= 1.f)
		{
			It.RemoveCurrent();
		}
	}
}

void AFixedCameraVolume::StartControlRotationBlend(APlayerController* PC, const FRotator& Target)
{
	FControlRotationBlend Blend;
	Blend.StartRotation = PC->GetControlRotation();
	Blend.TargetRotation = Target;
	Blend.Duration = ControlRotationBlendTime;
	ControlRotationBlends.Add(PC, Blend);
}

void AFixedCameraVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	FOccupant& Occupant = Occupants.FindOrAdd(Character);
	if (Occupant.OverlapCount++ > 0)
	{
		return;
	}

	// ── Camera switch (local players only) ──
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (PC && Character->IsLocallyControlled())
	{
		// Remember the PC: on death / unpossess the controller may already be gone at EndOverlap
		Occupant.LockedPC = PC;
		Occupant.bLockedInput = true;
		Occupant.SavedControlRotation = PC->GetControlRotation();

		PC->SetViewTargetWithBlend(this, BlendTime);
		StartControlRotationBlend(PC, FixedControlRotation);
		PC->SetIgnoreLookInput(true);
	}

	// ── Split screen transition (optional, this machine only) ──
	if (bUseSplitScreenTransition
		&& UDynamicSplitScreenSubsystem::ShouldLocalViewRespondTo(Character, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		Occupant.bRequestedFullScreen = true;

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->RequestFullScreen(this);
			}
		}
	}

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("FixedCameraVolume: %s entered -> Fixed camera"), *Character->GetName());
}

void AFixedCameraVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	FOccupant* Occupant = Occupants.Find(Character);
	if (!Occupant) return;

	if (--Occupant->OverlapCount > 0)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		PC = Occupant->LockedPC.Get();	// the lock must be released even if the controller is gone
	}

	// ── Restore camera (only if we locked it on enter) ──
	if (PC && Occupant->bLockedInput)
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);

		// Movement follows the control rotation yaw. Restoring the old yaw would make the direction jump on exit,
		// so by default keep the yaw and only restore pitch/roll.
		const FRotator CurrentRot = PC->GetControlRotation();
		FRotator TargetRot = CurrentRot;
		if (bRestoreControlRotationOnExit)
		{
			TargetRot = Occupant->SavedControlRotation;
		}
		else
		{
			TargetRot.Pitch = Occupant->SavedControlRotation.Pitch;
			TargetRot.Roll = Occupant->SavedControlRotation.Roll;
		}

		if (!TargetRot.Equals(CurrentRot, 0.01f))
		{
			StartControlRotationBlend(PC, TargetRot);
		}
		else
		{
			ControlRotationBlends.Remove(PC);	// stop the enter blend so it doesn't pull back
		}

		// Exactly 1:1 with the lock on enter
		PC->SetIgnoreLookInput(false);
	}

	const bool bWasFullScreenRequester = Occupant->bRequestedFullScreen;
	Occupants.Remove(Character);

	bool bAnyRequesterLeft = false;
	for (auto It = Occupants.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		bAnyRequesterLeft |= It->Value.bRequestedFullScreen;
	}

	// ── Restore split screen once every player that changed our screen has left ──
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

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("FixedCameraVolume: %s exited -> Restore 3rd-person camera"), *Character->GetName());
}
