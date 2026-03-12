// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/DynamicSplitScreenCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h"

ADynamicSplitScreenCharacter::ADynamicSplitScreenCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADynamicSplitScreenCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (AController* CharController = GetController())
		{
			ReplicatedCameraRotation = CharController->GetControlRotation();
		}
	}
}

void ADynamicSplitScreenCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADynamicSplitScreenCharacter, ReplicatedCameraRotation);
}
