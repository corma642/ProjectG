// Copyright Epic Games, Inc. All Rights Reserved.

#include "Environment/PGRunnerEnvironmentComponent.h"

#include "Cargo/PGCargoStackComponent.h"
#include "DrawDebugHelpers.h"

UPGRunnerEnvironmentComponent::UPGRunnerEnvironmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGRunnerEnvironmentComponent::InitializeEnvironment(const FPGEnvironmentSettings& InEnvironmentSettings, UPGCargoStackComponent* InCargoStackComponent)
{
	CachedEnvironmentSettings = InEnvironmentSettings;
	CargoStackComponent = InCargoStackComponent;
	ClearEnvironmentEffects();
	bIsRunActive = true;
}

void UPGRunnerEnvironmentComponent::SetRunActive(bool bInRunActive)
{
	bIsRunActive = bInRunActive;
	if (!bIsRunActive)
	{
		ClearEnvironmentEffects();
	}
}

float UPGRunnerEnvironmentComponent::UpdateEnvironment(float DeltaSeconds, float RawBalanceInput)
{
	if (!bIsRunActive)
	{
		ProcessedBalanceInput = 0.0f;
		ProcessedCargoBalanceInput = 0.0f;
		return ProcessedBalanceInput;
	}

	if (!HasActiveEnvironmentEffect())
	{
		ProcessedBalanceInput = RawBalanceInput;
		ProcessedCargoBalanceInput = RawBalanceInput;
		return ProcessedBalanceInput;
	}

	float EnvironmentBalanceInput = RawBalanceInput;
	float CargoBalanceInput = RawBalanceInput;

	if (SnowstormRemainingTime > 0.0f)
	{
		SnowstormRemainingTime = FMath::Max(0.0f, SnowstormRemainingTime - DeltaSeconds);

		// 눈바람은 외력이 아니라 입력 과민 효과다. 입력이 0이면 증폭 결과도 0이다.
		const float EventStrength = FMath::Max(0.0f, SnowstormStrength);
		const float InputScale = FMath::Max(1.0f, CachedEnvironmentSettings.SnowstormInputSensitivityScale) * EventStrength;
		EnvironmentBalanceInput = RawBalanceInput * InputScale;
		CargoBalanceInput = EnvironmentBalanceInput * FMath::Max(1.0f, CachedEnvironmentSettings.SnowstormCargoInputForceScale);
	}
	else
	{
		CargoBalanceInput = RawBalanceInput;
	}

	if (WindRemainingTime > 0.0f)
	{
		WindRemainingTime = FMath::Max(0.0f, WindRemainingTime - DeltaSeconds);

		// 바람은 플레이어 입력 위에 외부 중심 영향을 더해 적재물과 몸 기울기에 같은 방향성을 만든다.
		const float WindBias = WindDirection * WindStrength * CachedEnvironmentSettings.WindBalanceBiasScale;
		EnvironmentBalanceInput += WindBias;
		CargoBalanceInput += WindBias;
	}

	ProcessedBalanceInput = FMath::Clamp(EnvironmentBalanceInput, -1.0f, 1.0f);
	ProcessedCargoBalanceInput = CargoBalanceInput;
	DrawEnvironmentDebug();
	return ProcessedBalanceInput;
}

void UPGRunnerEnvironmentComponent::ApplyWind(float Direction, float Strength, float Duration)
{
	if (!bIsRunActive)
	{
		return;
	}

	if (FMath::IsNearlyZero(Direction) || Strength <= 0.0f || Duration <= 0.0f)
	{
		WindDirection = 0.0f;
		WindStrength = 0.0f;
		WindRemainingTime = 0.0f;
		return;
	}

	WindDirection = Direction > 0.0f ? 1.0f : -1.0f;
	WindStrength = Strength;
	WindRemainingTime = Duration;
}

void UPGRunnerEnvironmentComponent::ApplySnowstorm(float Duration, float Strength)
{
	if (!bIsRunActive)
	{
		return;
	}

	if (Duration <= 0.0f || Strength <= 0.0f)
	{
		SnowstormRemainingTime = 0.0f;
		SnowstormStrength = 0.0f;
		return;
	}

	SnowstormRemainingTime = Duration;
	SnowstormStrength = Strength;
}

bool UPGRunnerEnvironmentComponent::RequestCargoDrop()
{
	if (!bIsRunActive || !CargoStackComponent)
	{
		return false;
	}

	return CargoStackComponent->AddCargo();
}

void UPGRunnerEnvironmentComponent::ClearEnvironmentEffects()
{
	WindRemainingTime = 0.0f;
	WindDirection = 0.0f;
	WindStrength = 0.0f;
	SnowstormRemainingTime = 0.0f;
	SnowstormStrength = 0.0f;
	ProcessedBalanceInput = 0.0f;
	ProcessedCargoBalanceInput = 0.0f;
}

bool UPGRunnerEnvironmentComponent::HasActiveEnvironmentEffect() const
{
	return WindRemainingTime > 0.0f || SnowstormRemainingTime > 0.0f;
}

void UPGRunnerEnvironmentComponent::DrawEnvironmentDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!CachedEnvironmentSettings.bDrawEnvironmentDebug || !HasActiveEnvironmentEffect())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString DebugText = FString::Printf(TEXT("Wind %.2fs / Snowstorm %.2fs / Balance %.2f / Cargo %.2f"), WindRemainingTime, SnowstormRemainingTime, ProcessedBalanceInput, ProcessedCargoBalanceInput);
	DrawDebugString(World, GetComponentLocation() + FVector::UpVector * 160.0f, DebugText, nullptr, FColor::Cyan, 0.0f, true);
#endif
}
