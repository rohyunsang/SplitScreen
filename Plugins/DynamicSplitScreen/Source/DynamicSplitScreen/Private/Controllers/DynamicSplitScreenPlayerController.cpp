// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/DynamicSplitScreenPlayerController.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void ADynamicSplitScreenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoEnableSplitScreen && GetNetMode() == NM_Client && IsLocalController())
	{
		if (UDynamicSplitScreenSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UDynamicSplitScreenSubsystem>())
		{
			Subsystem->EnableSplitScreen();
		}
	}
}
