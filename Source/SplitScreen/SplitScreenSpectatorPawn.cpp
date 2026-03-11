// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplitScreenSpectatorPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"

ASplitScreenSpectatorPawn::ASplitScreenSpectatorPawn()
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
	CameraBoom->bDoCollisionTest = false; // 카메라가 캐릭터를 뚫고 충돌체크하다가 떨리는 현상 방지

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	SetActorHiddenInGame(true);
}

void ASplitScreenSpectatorPawn::BeginPlay()
{
	Super::BeginPlay();
}
