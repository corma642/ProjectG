// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

#include "Cargo/PGCargoTypeDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"

UPGCargoStackComponent::UPGCargoStackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGCargoStackComponent::InitializeCargoStack(const FPGCargoSettings& InCargoSettings)
{
	CachedCargoSettings = InCargoSettings;
	CargoInstances.Reset();
	CurrentSwayRoll = 0.0f;
	CurrentJumpLift = 0.0f;
	bWasChangingLaneLastFrame = false;

	const int32 InitialCargoCount = FMath::Clamp(CachedCargoSettings.InitialCargoCount, 0, FMath::Max(1, CachedCargoSettings.MaxCargoCount));
	for (int32 CargoIndex = 0; CargoIndex < InitialCargoCount; ++CargoIndex)
	{
		CargoInstances.Add(MakeDefaultCargoInstance());
	}

	EnsureCargoMeshPool();
	if (CachedCargoSettings.bDebugStartWithBonusCargo)
	{
		// 에디터에서 핵심/보너스 손실 흐름을 바로 비교할 수 있도록 선택적으로 보너스 적재물을 추가한다.
		AddBonusCargo();
	}

	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
}

void UPGCargoStackComponent::UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane)
{
	float MaxSwayMultiplier = 1.0f;
	for (const FPGCargoInstance& CargoInstance : CargoInstances)
	{
		MaxSwayMultiplier = FMath::Max(MaxSwayMultiplier, GetCargoSwayMultiplier(CargoInstance));
	}

	const float TargetSwayRoll = BalanceInput * CachedCargoSettings.SwayMaxRoll * MaxSwayMultiplier;
	CurrentSwayRoll = FMath::FInterpTo(CurrentSwayRoll, TargetSwayRoll, DeltaSeconds, CachedCargoSettings.SwayInterpSpeed);

	// 라인 이동 중에만 점프 상승 반응을 만들고, 착지 후에는 자연스럽게 기본 슬롯 위치로 돌아오게 한다.
	const float TargetJumpLift = bIsChangingLane ? VisualJumpOffset * CachedCargoSettings.JumpLiftScale : 0.0f;
	CurrentJumpLift = FMath::FInterpTo(CurrentJumpLift, TargetJumpLift, DeltaSeconds, CachedCargoSettings.SwayInterpSpeed);

	UpdateCargoStability(DeltaSeconds, BalanceInput, bIsChangingLane);
	UpdateSlotTransforms();

	bool bHasCargoWithoutMesh = false;
	for (const FPGCargoInstance& CargoInstance : CargoInstances)
	{
		if (!GetCargoMesh(CargoInstance))
		{
			bHasCargoWithoutMesh = true;
			break;
		}
	}

	if (CachedCargoSettings.bDrawCargoDebug || bHasCargoWithoutMesh)
	{
		DrawCargoDebug();
	}
}

bool UPGCargoStackComponent::AddCargo()
{
	if (CargoInstances.Num() >= CachedCargoSettings.MaxCargoCount)
	{
		return false;
	}

	CargoInstances.Add(MakeDefaultCargoInstance());
	EnsureCargoMeshPool();
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
	return true;
}

bool UPGCargoStackComponent::AddCargoByType(UPGCargoTypeDefinition* CargoType)
{
	if (!CargoType || CargoInstances.Num() >= CachedCargoSettings.MaxCargoCount)
	{
		return false;
	}

	CargoInstances.Add(MakeCargoInstance(CargoType, true));
	EnsureCargoMeshPool();
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
	return true;
}

bool UPGCargoStackComponent::AddBonusCargo()
{
	if (CargoInstances.Num() >= CachedCargoSettings.MaxCargoCount)
	{
		return false;
	}

	UPGCargoTypeDefinition* BonusCargoType = CachedCargoSettings.BonusCargoType.LoadSynchronous();
	FPGCargoInstance BonusCargoInstance = MakeCargoInstance(BonusCargoType, false);
	// 보너스 적재물은 DataAsset 설정 실수와 관계없이 페널티 검증 흐름을 보장해야 한다.
	BonusCargoInstance.bIsCoreCargoInstance = false;

	CargoInstances.Add(BonusCargoInstance);
	EnsureCargoMeshPool();
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
	return true;
}

bool UPGCargoStackComponent::RemoveCargo()
{
	if (CargoInstances.IsEmpty())
	{
		return false;
	}

	LoseCargoAt(CargoInstances.Num() - 1);
	return true;
}

float UPGCargoStackComponent::GetCargoStabilityValue(int32 SlotIndex) const
{
	return CargoInstances.IsValidIndex(SlotIndex) ? CargoInstances[SlotIndex].StabilityValue : 0.0f;
}

EPGCargoStabilityState UPGCargoStackComponent::GetCargoStabilityState(int32 SlotIndex) const
{
	return CargoInstances.IsValidIndex(SlotIndex) ? CargoInstances[SlotIndex].StabilityState : EPGCargoStabilityState::Stable;
}

FPGCargoInstance UPGCargoStackComponent::MakeDefaultCargoInstance() const
{
	return MakeCargoInstance(CachedCargoSettings.DefaultCargoType.LoadSynchronous(), true);
}

FPGCargoInstance UPGCargoStackComponent::MakeCargoInstance(UPGCargoTypeDefinition* CargoType, bool bFallbackCoreCargo) const
{
	FPGCargoInstance NewCargoInstance;
	NewCargoInstance.CargoType = CargoType;
	NewCargoInstance.bIsCoreCargoInstance = CargoType ? CargoType->bIsCoreCargo : bFallbackCoreCargo;
	NewCargoInstance.StabilityValue = 0.0f;
	NewCargoInstance.StabilityState = EPGCargoStabilityState::Stable;
	return NewCargoInstance;
}

void UPGCargoStackComponent::EnsureCargoMeshPool()
{
	const int32 RequiredPoolSize = FMath::Max(0, CachedCargoSettings.MaxCargoCount);
	for (int32 SlotIndex = CargoMeshComponents.Num(); SlotIndex < RequiredPoolSize; ++SlotIndex)
	{
		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(GetOwner());
		if (!MeshComponent)
		{
			continue;
		}

		// 적재물 메시 슬롯은 런타임 표시 전용으로 생성하고, 충돌은 추후 손실 판정 단계에서 별도로 다룬다.
		MeshComponent->CreationMethod = EComponentCreationMethod::Instance;
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetupAttachment(this);
		MeshComponent->RegisterComponent();
		MeshComponent->SetVisibility(false, true);
		MeshComponent->SetHiddenInGame(true, true);
		CargoMeshComponents.Add(MeshComponent);
	}
}

void UPGCargoStackComponent::RefreshCargoSlotVisibility()
{
	for (int32 SlotIndex = 0; SlotIndex < CargoMeshComponents.Num(); ++SlotIndex)
	{
		UStaticMeshComponent* MeshComponent = CargoMeshComponents[SlotIndex];
		if (!MeshComponent)
		{
			continue;
		}

		const bool bHasCargo = CargoInstances.IsValidIndex(SlotIndex);
		UStaticMesh* SlotMesh = bHasCargo ? GetCargoMesh(CargoInstances[SlotIndex]) : nullptr;
		const bool bShouldShowSlot = bHasCargo && SlotMesh;
		MeshComponent->SetStaticMesh(SlotMesh);
		MeshComponent->SetVisibility(bShouldShowSlot, true);
		MeshComponent->SetHiddenInGame(!bShouldShowSlot, true);
	}
}

void UPGCargoStackComponent::UpdateSlotTransforms()
{
	for (int32 SlotIndex = 0; SlotIndex < CargoInstances.Num() && SlotIndex < CargoMeshComponents.Num(); ++SlotIndex)
	{
		UStaticMeshComponent* MeshComponent = CargoMeshComponents[SlotIndex];
		if (!MeshComponent)
		{
			continue;
		}

		const float SwayMultiplier = GetCargoSwayMultiplier(CargoInstances[SlotIndex]);
		const FRotator SwayRotation(0.0f, 0.0f, CurrentSwayRoll * SwayMultiplier);
		MeshComponent->SetRelativeLocation(GetSlotRelativeLocation(SlotIndex));
		MeshComponent->SetRelativeRotation(SwayRotation);
	}
}

void UPGCargoStackComponent::UpdateCargoStability(float DeltaSeconds, float BalanceInput, bool bIsChangingLane)
{
	const bool bStartedLaneJump = bIsChangingLane && !bWasChangingLaneLastFrame;
	const float BalanceDamage = FMath::Abs(BalanceInput) * CachedCargoSettings.BalanceDamageRate * DeltaSeconds;
	const bool bShouldRecover = FMath::IsNearlyZero(BalanceInput) && !bIsChangingLane;

	for (int32 SlotIndex = CargoInstances.Num() - 1; SlotIndex >= 0; --SlotIndex)
	{
		FPGCargoInstance& CargoInstance = CargoInstances[SlotIndex];
		const float DamageMultiplier = GetCargoDamageMultiplier(CargoInstance);
		float NextStabilityValue = CargoInstance.StabilityValue + BalanceDamage * DamageMultiplier;

		if (bStartedLaneJump)
		{
			// 라인 점프 시작 시 적재물 재착지 긴장감을 만들기 위해 한 번만 추가 손상을 준다.
			NextStabilityValue += CachedCargoSettings.JumpDamageAmount * DamageMultiplier;
		}

		if (bShouldRecover)
		{
			NextStabilityValue -= CachedCargoSettings.StabilityRecoveryRate * DeltaSeconds;
		}

		CargoInstance.StabilityValue = FMath::Clamp(NextStabilityValue, 0.0f, CachedCargoSettings.LossThreshold);
		const EPGCargoStabilityState PreviousState = CargoInstance.StabilityState;
		CargoInstance.StabilityState = CalculateStabilityState(CargoInstance.StabilityValue);

		if (PreviousState != CargoInstance.StabilityState)
		{
			OnCargoStateChanged.Broadcast(SlotIndex, CargoInstance.StabilityState);
		}

		if (CargoInstance.StabilityValue >= CachedCargoSettings.LossThreshold)
		{
			LoseCargoAt(SlotIndex);
		}
	}

	bWasChangingLaneLastFrame = bIsChangingLane;
}

EPGCargoStabilityState UPGCargoStackComponent::CalculateStabilityState(float StabilityValue) const
{
	if (StabilityValue >= CachedCargoSettings.CriticalThreshold)
	{
		return EPGCargoStabilityState::Critical;
	}

	if (StabilityValue >= CachedCargoSettings.WarningThreshold)
	{
		return EPGCargoStabilityState::Warning;
	}

	return EPGCargoStabilityState::Stable;
}

float UPGCargoStackComponent::GetCargoDamageMultiplier(const FPGCargoInstance& CargoInstance) const
{
	return CargoInstance.CargoType ? FMath::Max(0.0f, CargoInstance.CargoType->StabilityDamageMultiplier) : 1.0f;
}

float UPGCargoStackComponent::GetCargoSwayMultiplier(const FPGCargoInstance& CargoInstance) const
{
	return CargoInstance.CargoType ? FMath::Max(0.0f, CargoInstance.CargoType->SwayMultiplier) : 1.0f;
}

bool UPGCargoStackComponent::IsCoreCargo(const FPGCargoInstance& CargoInstance) const
{
	return CargoInstance.bIsCoreCargoInstance;
}

UStaticMesh* UPGCargoStackComponent::GetCargoMesh(const FPGCargoInstance& CargoInstance) const
{
	return CargoInstance.CargoType ? CargoInstance.CargoType->CargoMesh.Get() : nullptr;
}

void UPGCargoStackComponent::LoseCargoAt(int32 SlotIndex)
{
	if (!CargoInstances.IsValidIndex(SlotIndex))
	{
		return;
	}

	const bool bWasCoreCargo = IsCoreCargo(CargoInstances[SlotIndex]);
	CargoInstances.RemoveAt(SlotIndex);
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
	OnCargoLost.Broadcast(SlotIndex, bWasCoreCargo);
}

void UPGCargoStackComponent::DrawCargoDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < CargoInstances.Num(); ++SlotIndex)
	{
		const FVector SlotWorldLocation = GetComponentTransform().TransformPosition(GetSlotRelativeLocation(SlotIndex));
		FColor SlotColor = FColor::Yellow;
		if (CargoInstances[SlotIndex].StabilityState == EPGCargoStabilityState::Warning)
		{
			SlotColor = FColor::Orange;
		}
		else if (CargoInstances[SlotIndex].StabilityState == EPGCargoStabilityState::Critical)
		{
			SlotColor = FColor::Red;
		}

		// 메시가 없어도 머리 위 적재 위치와 안정도 상태를 PIE에서 확인할 수 있게 표시한다.
		DrawDebugSphere(World, SlotWorldLocation, 10.0f, 12, SlotColor, false, 0.0f, 0, 1.5f);
		DrawDebugLine(World, SlotWorldLocation, SlotWorldLocation + GetUpVector() * 18.0f, SlotColor, false, 0.0f, 0, 1.0f);
	}
#endif
}

FVector UPGCargoStackComponent::GetSlotRelativeLocation(int32 SlotIndex) const
{
	const float SlotHeight = static_cast<float>(SlotIndex) * CachedCargoSettings.CargoSlotSpacing;
	return CachedCargoSettings.CargoBaseOffset + FVector(0.0f, 0.0f, SlotHeight + CurrentJumpLift);
}
