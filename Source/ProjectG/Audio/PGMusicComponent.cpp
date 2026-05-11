// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/PGMusicComponent.h"

#include "Components/AudioComponent.h"
#include "GameModes/PGGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/PGRuntimePreloadSubsystem.h"
#include "Sound/SoundBase.h"

UPGMusicComponent::UPGMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGMusicComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadSettings();
	BindGameMode();
	BindRuntimePreloadIfNeeded();
	if (bIsBoundToRuntimePreload)
	{
		return;
	}

	RefreshMusicForCurrentState();
}

void UPGMusicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameMode();
	UnbindRuntimePreload();
	FadeOutAllMusic();
	Super::EndPlay(EndPlayReason);
}

void UPGMusicComponent::SetMusicDucked(bool bDucked)
{
	if (bIsMusicDucked == bDucked)
	{
		return;
	}

	bIsMusicDucked = bDucked;
	ApplyRunMusicVolume(CachedMusicSettings.PauseDuckFadeDuration);
}

void UPGMusicComponent::HandleRunCountdownStarted(float Duration)
{
	BindRuntimePreloadIfNeeded();
	if (bIsBoundToRuntimePreload)
	{
		return;
	}

	PlayRunMusic();
}

void UPGMusicComponent::HandleRunStarted()
{
	BindRuntimePreloadIfNeeded();
	if (bIsBoundToRuntimePreload)
	{
		return;
	}

	PlayRunMusic();
}

void UPGMusicComponent::HandleRunFailed(FName FailureReason)
{
	FadeOutAllMusic();
}

void UPGMusicComponent::HandleRuntimePreloadCompleted()
{
	UnbindRuntimePreload();
	RefreshMusicForCurrentState();
}

void UPGMusicComponent::LoadSettings()
{
	if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
	{
		CachedMusicSettings = Settings->Music;
	}
}

void UPGMusicComponent::BindGameMode()
{
	if (bIsBoundToGameMode || !GetWorld())
	{
		return;
	}

	BoundGameMode = GetWorld()->GetAuthGameMode<APGGameModeBase>();
	if (!BoundGameMode)
	{
		return;
	}

	BoundGameMode->OnRunCountdownStarted.AddDynamic(this, &UPGMusicComponent::HandleRunCountdownStarted);
	BoundGameMode->OnRunStarted.AddDynamic(this, &UPGMusicComponent::HandleRunStarted);
	BoundGameMode->OnRunFailed.AddDynamic(this, &UPGMusicComponent::HandleRunFailed);
	bIsBoundToGameMode = true;
}

void UPGMusicComponent::UnbindGameMode()
{
	if (BoundGameMode && bIsBoundToGameMode)
	{
		BoundGameMode->OnRunCountdownStarted.RemoveDynamic(this, &UPGMusicComponent::HandleRunCountdownStarted);
		BoundGameMode->OnRunStarted.RemoveDynamic(this, &UPGMusicComponent::HandleRunStarted);
		BoundGameMode->OnRunFailed.RemoveDynamic(this, &UPGMusicComponent::HandleRunFailed);
	}

	BoundGameMode = nullptr;
	bIsBoundToGameMode = false;
}

void UPGMusicComponent::BindRuntimePreloadIfNeeded()
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	if (!PreloadSubsystem || PreloadSubsystem->IsReady() || bIsBoundToRuntimePreload)
	{
		return;
	}

	PreloadSubsystem->OnPreloadCompleted.AddDynamic(this, &UPGMusicComponent::HandleRuntimePreloadCompleted);
	bIsBoundToRuntimePreload = true;
}

void UPGMusicComponent::UnbindRuntimePreload()
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UPGRuntimePreloadSubsystem* PreloadSubsystem = GameInstance ? GameInstance->GetSubsystem<UPGRuntimePreloadSubsystem>() : nullptr;
	if (PreloadSubsystem && bIsBoundToRuntimePreload)
	{
		PreloadSubsystem->OnPreloadCompleted.RemoveDynamic(this, &UPGMusicComponent::HandleRuntimePreloadCompleted);
	}

	bIsBoundToRuntimePreload = false;
}

void UPGMusicComponent::RefreshMusicForCurrentState()
{
	if (!BoundGameMode)
	{
		return;
	}

	switch (BoundGameMode->GetRunState())
	{
	case EPGRunState::Home:
		if (!IsRetryCountdownLoad())
		{
			PlayHomeMusic();
		}
		break;
	case EPGRunState::Countdown:
	case EPGRunState::Running:
		PlayRunMusic();
		break;
	case EPGRunState::Failed:
		FadeOutAllMusic();
		break;
	default:
		break;
	}
}

void UPGMusicComponent::PlayHomeMusic()
{
	PlayMusic(CachedMusicSettings.HomeMusic.Get(), HomeMusicComponent, RunMusicComponent);
}

void UPGMusicComponent::PlayRunMusic()
{
	PlayMusic(CachedMusicSettings.RunMusic.Get(), RunMusicComponent, HomeMusicComponent);
	ApplyRunMusicVolume(CachedMusicSettings.MusicFadeDuration);
}

void UPGMusicComponent::PlayMusic(USoundBase* Music, TObjectPtr<UAudioComponent>& TargetComponent, TObjectPtr<UAudioComponent>& OtherComponent)
{
	FadeOutMusic(OtherComponent);

	if (!Music)
	{
		FadeOutMusic(TargetComponent);
		return;
	}

	if (TargetComponent && TargetComponent->IsPlaying())
	{
		return;
	}

	TargetComponent = UGameplayStatics::CreateSound2D(
		this,
		Music,
		CachedMusicSettings.MusicVolume,
		1.0f,
		0.0f,
		nullptr,
		false,
		false);

	if (!TargetComponent)
	{
		return;
	}

	TargetComponent->bIsUISound = true;
	TargetComponent->FadeIn(FMath::Max(0.0f, CachedMusicSettings.MusicFadeDuration), CachedMusicSettings.MusicVolume);
}

void UPGMusicComponent::FadeOutMusic(TObjectPtr<UAudioComponent>& MusicComponent)
{
	if (!MusicComponent)
	{
		return;
	}

	MusicComponent->FadeOut(FMath::Max(0.0f, CachedMusicSettings.MusicFadeDuration), 0.0f);
	MusicComponent->bAutoDestroy = true;
	MusicComponent = nullptr;
}

void UPGMusicComponent::FadeOutAllMusic()
{
	FadeOutMusic(HomeMusicComponent);
	FadeOutMusic(RunMusicComponent);
}

void UPGMusicComponent::ApplyRunMusicVolume(float FadeDuration)
{
	if (!RunMusicComponent)
	{
		return;
	}

	const float TargetVolume = bIsMusicDucked ? CachedMusicSettings.PauseDuckedVolume : CachedMusicSettings.MusicVolume;
	RunMusicComponent->AdjustVolume(FMath::Max(0.0f, FadeDuration), TargetVolume);
}

bool UPGMusicComponent::IsRetryCountdownLoad() const
{
	const UWorld* World = GetWorld();
	const FString StartFlowOption = World ? World->URL.GetOption(TEXT("StartFlow="), TEXT("")) : FString();
	return StartFlowOption.Equals(TEXT("RetryCountdown"), ESearchCase::IgnoreCase);
}
