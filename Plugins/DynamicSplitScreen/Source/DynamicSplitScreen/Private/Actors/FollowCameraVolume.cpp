// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/FollowCameraVolume.h"
#include "DynamicSplitScreen.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"

// ─── AFollowCameraActor ───────────────────────────────────────

AFollowCameraActor::AFollowCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	RootComponent = CameraComponent;
}

void AFollowCameraActor::InitFollow(ACharacter* InTarget, const FVector& InOffset, const FRotator& InRotation, bool bInFollowCharacterYaw)
{
	TargetCharacter = InTarget;
	FollowOffset = InOffset;
	FixedRotation = InRotation;
	bFollowCharacterYaw = bInFollowCharacterYaw;

	if (TargetCharacter)
	{
		const float CharYaw = TargetCharacter->GetActorRotation().Yaw;

		const FVector WorldOffset = bFollowCharacterYaw
			? FRotator(0.f, CharYaw, 0.f).RotateVector(FollowOffset)
			: FollowOffset;

		const FRotator WorldRotation = bFollowCharacterYaw
			? FRotator(FixedRotation.Pitch, CharYaw + FixedRotation.Yaw, FixedRotation.Roll)
			: FixedRotation;

		SetActorLocation(TargetCharacter->GetActorLocation() + WorldOffset);
		SetActorRotation(WorldRotation);
	}
}

void AFollowCameraActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsValid(TargetCharacter)) return;

	const float CharYaw = TargetCharacter->GetActorRotation().Yaw;

	const FVector WorldOffset = bFollowCharacterYaw
		? FRotator(0.f, CharYaw, 0.f).RotateVector(FollowOffset)
		: FollowOffset;

	const FRotator WorldRotation = bFollowCharacterYaw
		? FRotator(FixedRotation.Pitch, CharYaw + FixedRotation.Yaw, FixedRotation.Roll)
		: FixedRotation;

	SetActorLocation(TargetCharacter->GetActorLocation() + WorldOffset);
	SetActorRotation(WorldRotation);
}

// ─── AFollowCameraVolume ──────────────────────────────────────

AFollowCameraVolume::AFollowCameraVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AFollowCameraVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AFollowCameraVolume::OnTriggerEndOverlap);
}

void AFollowCameraVolume::BeginPlay()
{
	Super::BeginPlay();
	Occupants.Empty();
}

void AFollowCameraVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	FOccupant& Occupant = Occupants.FindOrAdd(Character);
	if (Occupant.OverlapCount++ > 0)
	{
		return;
	}

	// ── Spawn follow camera and switch view target (local players only) ──
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (PC && Character->IsLocallyControlled())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		AFollowCameraActor* FollowCam = GetWorld()->SpawnActor<AFollowCameraActor>(SpawnParams);
		if (FollowCam)
		{
			FollowCam->InitFollow(Character, FollowOffset, FixedCameraRotation, bFollowCharacterYaw);
			Occupant.FollowCamera = FollowCam;
			Occupant.LockedPC = PC;

			PC->SetViewTargetWithBlend(FollowCam, BlendTime);

			if (bIgnoreLookInput)
			{
				PC->SetIgnoreLookInput(true);
				Occupant.bLockedInput = true;
			}
		}
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

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("FollowCameraVolume: %s entered -> Follow camera activated"), *Character->GetName());
}

void AFollowCameraVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	FOccupant* Occupant = Occupants.Find(Character);
	if (!Occupant) return;

	if (--Occupant->OverlapCount > 0)
	{
		return;
	}

	// ── Restore original camera ──
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		PC = Occupant->LockedPC.Get();
	}

	if (PC && Occupant->FollowCamera.IsValid())
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);
	}

	if (PC && Occupant->bLockedInput)
	{
		PC->SetIgnoreLookInput(false);
	}

	if (AFollowCameraActor* FollowCam = Occupant->FollowCamera.Get())
	{
		FollowCam->Destroy();
	}

	const bool bWasFullScreenRequester = Occupant->bRequestedFullScreen;
	Occupants.Remove(Character);

	bool bAnyRequesterLeft = false;
	for (auto It = Occupants.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (AFollowCameraActor* Orphan = It->Value.FollowCamera.Get())
			{
				Orphan->Destroy();
			}
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

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("FollowCameraVolume: %s exited -> Restored original camera"), *Character->GetName());
}
