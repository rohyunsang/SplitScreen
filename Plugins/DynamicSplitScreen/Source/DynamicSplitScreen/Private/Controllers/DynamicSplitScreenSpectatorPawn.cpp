// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/DynamicSplitScreenSpectatorPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"

ADynamicSplitScreenSpectatorPawn::ADynamicSplitScreenSpectatorPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bAddDefaultMovementBindings = false;

	if (USphereComponent* SphereComp = GetCollisionComponent())
	{
		SphereComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SphereComp->SetCollisionProfileName(FName(TEXT("NoCollision")));
		SphereComp->SetGenerateOverlapEvents(false);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	SetActorHiddenInGame(true);
}

void ADynamicSplitScreenSpectatorPawn::BeginPlay()
{
	Super::BeginPlay();
}
