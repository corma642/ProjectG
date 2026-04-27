// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/PGGameModeBase.h"

#include "Character/PGRunnerCharacter.h"
#include "Player/PGPlayerController.h"

APGGameModeBase::APGGameModeBase()
{
	DefaultPawnClass = APGRunnerCharacter::StaticClass();
	PlayerControllerClass = APGPlayerController::StaticClass();
}

void APGGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	RunState = EPGRunState::Running;
}

void APGGameModeBase::RequestRunFailed(FName FailureReason)
{
	if (RunState == EPGRunState::Failed)
	{
		return;
	}

	RunState = EPGRunState::Failed;
	OnRunFailed.Broadcast(FailureReason);
}

void APGGameModeBase::RequestCargoPenalty(int32 LostSlotIndex)
{
	if (RunState != EPGRunState::Running)
	{
		return;
	}

	OnCargoPenalty.Broadcast(LostSlotIndex);
}
