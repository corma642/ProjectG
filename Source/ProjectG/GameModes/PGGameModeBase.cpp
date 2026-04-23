// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Player/PGPlayerController.h"

APGGameModeBase::APGGameModeBase()
{
	DefaultPawnClass = APGRunnerCharacter::StaticClass();
	PlayerControllerClass = APGPlayerController::StaticClass();
}
