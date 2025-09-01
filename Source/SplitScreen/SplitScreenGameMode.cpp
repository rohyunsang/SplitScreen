// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplitScreenGameMode.h"
#include "SplitScreenCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASplitScreenGameMode::ASplitScreenGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
