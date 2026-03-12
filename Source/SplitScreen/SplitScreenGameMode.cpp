// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplitScreenGameMode.h"
#include "SplitScreenPlayerController.h"
#include "SplitScreenSpectatorPawn.h"
#include "UObject/ConstructorHelpers.h"

ASplitScreenGameMode::ASplitScreenGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	PlayerControllerClass = ASplitScreenPlayerController::StaticClass();
	DummyPlayerControllerClass = ASplitScreenPlayerController::StaticClass();
	DummySpectatorPawnClass = ASplitScreenSpectatorPawn::StaticClass();
}
