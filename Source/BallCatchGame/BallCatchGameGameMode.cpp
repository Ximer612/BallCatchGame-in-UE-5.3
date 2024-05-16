// Copyright Epic Games, Inc. All Rights Reserved.

#include "BallCatchGameGameMode.h"
#include "BallCatchGameCharacter.h"
#include "UObject/ConstructorHelpers.h"

ABallCatchGameGameMode::ABallCatchGameGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
