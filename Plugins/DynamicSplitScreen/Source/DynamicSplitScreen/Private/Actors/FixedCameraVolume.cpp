// Copyright Epic Games, Inc. All Rights Reserved.


#include "Actors/FixedCameraVolume.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"

AFixedCameraVolume::AFixedCameraVolume()
{
	PrimaryActorTick.bCanEverTick = false;

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
	PlayersInTrigger = 0;
}

void AFixedCameraVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger++;

	// ── Camera Switch (Local Players Only) ──
	if (Character->IsLocallyControlled())
	{
		PC->SetViewTargetWithBlend(this, BlendTime);

		// Store old rotation and lock to fixed rotation
		SavedControlRotations.Add(PC, PC->GetControlRotation());
		PC->SetControlRotation(FixedControlRotation);
		PC->SetIgnoreLookInput(true);
	}

	// ── Split Screen Transition (Optional) ──
	if (bUseSplitScreenTransition)
	{
		int32 TargetPlayerIndex = FixedFullScreenPlayerIndex;

		if (bFullScreenForEnteringPlayer)
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				TargetPlayerIndex = LP->GetControllerId();
			}
		}

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->TransitionToFullScreen(TargetPlayerIndex);
				UE_LOG(LogTemp, Log, TEXT("CameraVolume: Player %d -> Full Screen transition"), TargetPlayerIndex);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CameraVolume: %s entered -> Fixed camera, ControlRotation locked"), *Character->GetName());
}

void AFixedCameraVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	// ── Restore Camera (Local Players Only) ──
	if (Character->IsLocallyControlled())
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);

		if (FRotator* SavedRot = SavedControlRotations.Find(PC))
		{
			PC->SetControlRotation(*SavedRot);
			SavedControlRotations.Remove(PC);
		}

		PC->SetIgnoreLookInput(false);
	}

	// ── Restore Split Screen (When all players have left) ──
	if (bUseSplitScreenTransition && PlayersInTrigger <= 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->TransitionToSplitScreen();
				UE_LOG(LogTemp, Log, TEXT("CameraVolume: All players left -> Split Screen transition"));
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CameraVolume: %s exited -> Restore 3rd-person camera"), *Character->GetName());
}
