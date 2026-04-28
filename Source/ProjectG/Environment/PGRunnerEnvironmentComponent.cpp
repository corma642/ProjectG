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
		// 환경 효과가 없을 때는 원본 중심 입력을 그대로 통과시켜 Sprint 1/2 조작 비용과 감각을 유지한다.
		DelayedBalanceInput = RawBalanceInput;
		ProcessedBalanceInput = RawBalanceInput;
		return ProcessedBalanceInput;
	}

	float EnvironmentBalanceInput = RawBalanceInput;

	if (IceRemainingTime > 0.0f)
	{
		IceRemainingTime = FMath::Max(0.0f, IceRemainingTime - DeltaSeconds);

		// 얼음 길은 입력 자체를 막지 않고, 목표 입력으로 천천히 따라가게 만들어 미끄러지는 느낌을 준다.
		const float InterpSpeed = 12.0f * FMath::Clamp(IceInputLagScale, 0.05f, 1.0f);
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
		// 방향/강도/시간이 유효하지 않은 바람 요청은 이전 바람이 남지 않도록 명시적으로 정리한다.
		WindDirection = 0.0f;
		WindStrength = 0.0f;
		WindRemainingTime = 0.0f;
		return;
	}

	WindDirection = Direction > 0.0f ? 1.0f : -1.0f;
	WindStrength = Strength;
	WindRemainingTime = Duration;
}

void UPGRunnerEnvironmentComponent::ApplyIce(float Duration, float InputLagScale)
{
	if (!bIsRunActive)
	{
		return;
	}

	IceRemainingTime = FMath::Max(0.0f, Duration);
	IceInputLagScale = InputLagScale > 0.0f ? InputLagScale : CachedEnvironmentSettings.IceInputLagScale;
	IceInputLagScale = FMath::Clamp(IceInputLagScale, 0.05f, 1.0f);
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
	IceRemainingTime = 0.0f;
	IceInputLagScale = FMath::Clamp(CachedEnvironmentSettings.IceInputLagScale, 0.05f, 1.0f);
	DelayedBalanceInput = 0.0f;
	ProcessedBalanceInput = 0.0f;
}

bool UPGRunnerEnvironmentComponent::HasActiveEnvironmentEffect() const
{
	return WindRemainingTime > 0.0f || IceRemainingTime > 0.0f;
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

	const FString DebugText = FString::Printf(TEXT("Wind %.2fs / Ice %.2fs / Balance %.2f"), WindRemainingTime, IceRemainingTime, ProcessedBalanceInput);
	DrawDebugString(World, GetComponentLocation() + FVector::UpVector * 160.0f, DebugText, nullptr, FColor::Cyan, 0.0f, true);
#endif
}
