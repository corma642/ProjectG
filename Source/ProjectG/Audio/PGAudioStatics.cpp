// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/PGAudioStatics.h"

#include "Kismet/GameplayStatics.h"
#include "Settings/PGGameSettings.h"
#include "Settings/PGGameUserSettings.h"
#include "Sound/SoundBase.h"

void UPGAudioStatics::PlayCommonButtonClick(const UObject* WorldContextObject)
{
	const UPGGameSettings* Settings = GetDefault<UPGGameSettings>();
	PlaySFX2D(WorldContextObject, Settings ? Settings->SFX.CommonButtonClickSound.Get() : nullptr);
}

void UPGAudioStatics::PlaySFX2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier)
{
	if (!WorldContextObject || !Sound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(WorldContextObject, Sound, UPGGameUserSettings::GetCurrentSFXVolumeScale() * FMath::Max(0.0f, VolumeMultiplier));
}

void UPGAudioStatics::PlaySFXAtLocation(const UObject* WorldContextObject, USoundBase* Sound, const FVector& Location, float VolumeMultiplier)
{
	if (!WorldContextObject || !Sound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Location, UPGGameUserSettings::GetCurrentSFXVolumeScale() * FMath::Max(0.0f, VolumeMultiplier));
}
