// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

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
	CurrentCargoCount = FMath::Clamp(CachedCargoSettings.InitialCargoCount, 0, FMath::Max(1, CachedCargoSettings.MaxCargoCount));
	CurrentSwayRoll = 0.0f;
	CurrentJumpLift = 0.0f;

	EnsureCargoMeshPool();
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
}

void UPGCargoStackComponent::UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane)
{
	const float TargetSwayRoll = BalanceInput * CachedCargoSettings.SwayMaxRoll;
	CurrentSwayRoll = FMath::FInterpTo(CurrentSwayRoll, TargetSwayRoll, DeltaSeconds, CachedCargoSettings.SwayInterpSpeed);

	// 라인 이동 중에만 점프 상승 반응을 만들고, 착지 후에는 자연스럽게 기본 슬롯 위치로 돌아오게 한다.
	const float TargetJumpLift = bIsChangingLane ? VisualJumpOffset * CachedCargoSettings.JumpLiftScale : 0.0f;
	CurrentJumpLift = FMath::FInterpTo(CurrentJumpLift, TargetJumpLift, DeltaSeconds, CachedCargoSettings.SwayInterpSpeed);

	UpdateSlotTransforms();

	if (CachedCargoSettings.bDrawCargoDebug || !CargoMesh)
	{
		DrawCargoDebug();
	}
}

bool UPGCargoStackComponent::AddCargo()
{
	if (CurrentCargoCount >= CachedCargoSettings.MaxCargoCount)
	{
		return false;
	}

	++CurrentCargoCount;
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
	return true;
}

bool UPGCargoStackComponent::RemoveCargo()
{
	if (CurrentCargoCount <= 0)
	{
		return false;
	}

	--CurrentCargoCount;
	RefreshCargoSlotVisibility();
	UpdateSlotTransforms();
	return true;
}

void UPGCargoStackComponent::EnsureCargoMeshPool()
{
	if (!CargoMesh)
	{
		return;
	}

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
		MeshComponent->SetStaticMesh(CargoMesh);
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

		const bool bShouldShowSlot = CargoMesh && SlotIndex < CurrentCargoCount;
		MeshComponent->SetStaticMesh(CargoMesh);
		MeshComponent->SetVisibility(bShouldShowSlot, true);
		MeshComponent->SetHiddenInGame(!bShouldShowSlot, true);
	}
}

void UPGCargoStackComponent::UpdateSlotTransforms()
{
	const FRotator SwayRotation(0.0f, 0.0f, CurrentSwayRoll);

	for (int32 SlotIndex = 0; SlotIndex < CurrentCargoCount && SlotIndex < CargoMeshComponents.Num(); ++SlotIndex)
	{
		UStaticMeshComponent* MeshComponent = CargoMeshComponents[SlotIndex];
		if (!MeshComponent)
		{
			continue;
		}

		MeshComponent->SetRelativeLocation(GetSlotRelativeLocation(SlotIndex));
		MeshComponent->SetRelativeRotation(SwayRotation);
	}
}

void UPGCargoStackComponent::DrawCargoDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < CurrentCargoCount; ++SlotIndex)
	{
		const FVector SlotWorldLocation = GetComponentTransform().TransformPosition(GetSlotRelativeLocation(SlotIndex));
		const FColor SlotColor = SlotIndex == 0 ? FColor::Yellow : FColor::Orange;

		// 메시가 없어도 머리 위 적재 위치와 흔들림 상태를 PIE에서 확인할 수 있게 표시한다.
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
