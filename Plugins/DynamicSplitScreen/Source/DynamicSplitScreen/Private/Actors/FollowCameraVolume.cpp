// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/FollowCameraVolume.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
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
	PlayersInTrigger = 0;
}

void AFollowCameraVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger++;

	// ── Spawn follow camera and switch view target (Local Players Only) ──
	if (Character->IsLocallyControlled())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		AFollowCameraActor* FollowCam = GetWorld()->SpawnActor<AFollowCameraActor>(SpawnParams);
		if (FollowCam)
		{
			FollowCam->InitFollow(Character, FollowOffset, FixedCameraRotation, bFollowCharacterYaw);
			SpawnedCameras.Add(PC, FollowCam);

			PC->SetViewTargetWithBlend(FollowCam, BlendTime);

			if (bIgnoreLookInput)
			{
				PC->SetIgnoreLookInput(true);
			}
		}
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
				UE_LOG(LogTemp, Log, TEXT("FollowCameraVolume: Player %d -> Full Screen transition"), TargetPlayerIndex);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FollowCameraVolume: %s entered -> Follow camera activated"), *Character->GetName());
}

void AFollowCameraVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	// ── Restore original camera (Local Players Only) ──
	if (Character->IsLocallyControlled())
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);

		if (bIgnoreLookInput)
		{
			PC->SetIgnoreLookInput(false);
		}

		if (TObjectPtr<AFollowCameraActor>* FollowCam = SpawnedCameras.Find(PC))
		{
			if (IsValid(*FollowCam))
			{
				(*FollowCam)->Destroy();
			}
			SpawnedCameras.Remove(PC);
		}
	}

	// ── Restore Split Screen (When all players have left) ──
	if (bUseSplitScreenTransition && PlayersInTrigger <= 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDynamicSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UDynamicSplitScreenSubsystem>())
			{
				Subsystem->TransitionToSplitScreen();
				UE_LOG(LogTemp, Log, TEXT("FollowCameraVolume: All players left -> Restore split screen"));
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FollowCameraVolume: %s exited -> Restored original camera"), *Character->GetName());
}
