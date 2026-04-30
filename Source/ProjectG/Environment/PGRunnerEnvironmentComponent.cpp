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
		return ProcessedBalanceInput;
	}

	if (!HasActiveEnvironmentEffect())
	{
		DelayedBalanceInput = RawBalanceInput;
		ProcessedBalanceInput = RawBalanceInput;
		return ProcessedBalanceInput;
	}

	float EnvironmentBalanceInput = RawBalanceInput;

	if (SnowstormRemainingTime > 0.0f)
	{
		SnowstormRemainingTime = FMath::Max(0.0f, SnowstormRemainingTime - DeltaSeconds);

		// 눈바람은 노면을 바꾸지 않고, 차가운 바람과 시야 방해로 중심 보정 반응이 둔해지는 감각을 만든다.
		const float InterpSpeed = 12.0f * FMath::Clamp(SnowstormInputLagScale, 0.05f, 1.0f);
		DelayedBalanceInput = FMath::FInterpTo(DelayedBalanceInput, RawBalanceInput, DeltaSeconds, InterpSpeed);
		EnvironmentBalanceInput = DelayedBalanceInput;
	}
	else
	{
		DelayedBalanceInput = RawBalanceInput;
	}

	if (WindRemainingTime > 0.0f)
	{
		WindRemainingTime = FMath::Max(0.0f, WindRemainingTime - DeltaSeconds);

		// 바람은 플레이어 입력 위에 외부 중심 영향을 더해 적재물과 몸 기울기에 같은 방향성을 만든다.
		const float WindBias = WindDirection * WindStrength * CachedEnvironmentSettings.WindBalanceBiasScale;
		EnvironmentBalanceInput += WindBias;
	}

	ProcessedBalanceInput = FMath::Clamp(EnvironmentBalanceInput, -1.0f, 1.0f);
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

void UPGRunnerEnvironmentComponent::ApplySnowstorm(float Duration, float InputLagScale)
{
	if (!bIsRunActive)
	{
		return;
	}

	SnowstormRemainingTime = FMath::Max(0.0f, Duration);
	SnowstormInputLagScale = InputLagScale > 0.0f ? InputLagScale : CachedEnvironmentSettings.SnowstormInputLagScale;
	SnowstormInputLagScale = FMath::Clamp(SnowstormInputLagScale, 0.05f, 1.0f);
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
	SnowstormInputLagScale = FMath::Clamp(CachedEnvironmentSettings.SnowstormInputLagScale, 0.05f, 1.0f);
	DelayedBalanceInput = 0.0f;
	ProcessedBalanceInput = 0.0f;
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

	const FString DebugText = FString::Printf(TEXT("Wind %.2fs / Snowstorm %.2fs / Balance %.2f"), WindRemainingTime, SnowstormRemainingTime, ProcessedBalanceInput);
	DrawDebugString(World, GetComponentLocation() + FVector::UpVector * 160.0f, DebugText, nullptr, FColor::Cyan, 0.0f, true);
#endif
}
