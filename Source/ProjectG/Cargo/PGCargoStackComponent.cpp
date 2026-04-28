// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

#include "Cargo/PGCargoTypeDefinition.h"
#include "Cargo/PGPhysicsCargoActor.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UPGCargoStackComponent::UPGCargoStackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	PreviewCargoMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoPreview"));
	PreviewCargoMeshComponent->SetupAttachment(this);
	PreviewCargoMeshComponent->SetMobility(EComponentMobility::Movable);
	PreviewCargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewCargoMeshComponent->SetGenerateOverlapEvents(false);
	PreviewCargoMeshComponent->SetVisibility(false, true);
	PreviewCargoMeshComponent->SetHiddenInGame(true, true);
}

void UPGCargoStackComponent::InitializeCargoStack(const FPGCargoSettings& InCargoSettings)
{
	CachedCargoSettings = InCargoSettings;
	CargoActors.Reset();
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoPreviewRemainingTime = 0.0f;
	CargoShakeElapsedTime = 0.0f;

	SpawnInitialCargo();
}

void UPGCargoStackComponent::UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane)
{
	CleanupInvalidCargoActors();
	ApplyCargoPhysicsForces(DeltaSeconds, BalanceInput);

	if (bHasPendingCargoDrop)
	{
		CargoPreviewRemainingTime = FMath::Max(0.0f, CargoPreviewRemainingTime - DeltaSeconds);
		if (CargoPreviewRemainingTime <= 0.0f)
		{
			FinishCargoPreview();
		}
	}

	CheckCargoLoss();
	DrawCargoDebug();
}

bool UPGCargoStackComponent::AddCargo()
{
	return StartCargoPreview(ResolveDefaultCargoType());
}

bool UPGCargoStackComponent::AddCargoByType(UPGCargoTypeDefinition* CargoType)
{
	return StartCargoPreview(CargoType);
}

bool UPGCargoStackComponent::RemoveCargo()
{
	if (CargoActors.IsEmpty())
	{
		return false;
	}

	LoseCargoAt(CargoActors.Num() - 1);
	return true;
}

void UPGCargoStackComponent::SpawnInitialCargo()
{
	UPGCargoTypeDefinition* CargoType = ResolveDefaultCargoType();
	if (!CargoType || !CargoType->CargoMesh)
	{
		return;
	}

	const int32 InitialCount = FMath::Clamp(CachedCargoSettings.InitialCargoCount, 0, FMath::Max(1, CachedCargoSettings.MaxCargoCount));
	for (int32 CargoIndex = 0; CargoIndex < InitialCount; ++CargoIndex)
	{
		SpawnPhysicsCargo(CargoType, MakeCargoWorldTransform(CachedCargoSettings.InitialCargoLocalOffset, CargoIndex));
	}
}

bool UPGCargoStackComponent::StartCargoPreview(UPGCargoTypeDefinition* CargoType)
{
	if (!CargoType || !CargoType->CargoMesh)
	{
		return false;
	}

	const int32 ReservedCargoCount = CargoActors.Num() + (bHasPendingCargoDrop ? 1 : 0);
	if (ReservedCargoCount >= CachedCargoSettings.MaxCargoCount)
	{
		return false;
	}

	PendingCargoType = CargoType;
	bHasPendingCargoDrop = true;
	CargoPreviewRemainingTime = FMath::Max(0.0f, CachedCargoSettings.CargoPreviewDuration);

	if (PreviewCargoMeshComponent)
	{
		// 프리뷰는 이 컴포넌트 하위에 남겨 플레이어가 전진해도 현재 플레이어 기준 위치를 따라가게 한다.
		PreviewCargoMeshComponent->SetStaticMesh(CargoType->CargoMesh);
		PreviewCargoMeshComponent->SetRelativeLocation(CachedCargoSettings.CargoPreviewLocalOffset);
		PreviewCargoMeshComponent->SetRelativeRotation(CargoType->MeshRotation);
		PreviewCargoMeshComponent->SetRelativeScale3D(CargoType->MeshScale);
		PreviewCargoMeshComponent->SetVisibility(true, true);
		PreviewCargoMeshComponent->SetHiddenInGame(false, true);
	}

	if (CargoPreviewRemainingTime <= 0.0f)
	{
		FinishCargoPreview();
	}

	return true;
}

void UPGCargoStackComponent::FinishCargoPreview()
{
	if (!bHasPendingCargoDrop)
	{
		return;
	}

	UPGCargoTypeDefinition* CargoType = PendingCargoType;
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoPreviewRemainingTime = 0.0f;

	if (PreviewCargoMeshComponent)
	{
		PreviewCargoMeshComponent->SetVisibility(false, true);
		PreviewCargoMeshComponent->SetHiddenInGame(true, true);
	}

	// 실제 계란은 프리뷰 종료 순간의 플레이어 앵커 위치에서 월드 Actor로 스폰하고, 이후 플레이어에 부착하지 않는다.
	SpawnPhysicsCargo(CargoType, MakeCargoWorldTransform(CachedCargoSettings.CargoPreviewLocalOffset));
}

APGPhysicsCargoActor* UPGCargoStackComponent::SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform)
{
	if (!CargoType || !CargoType->CargoMesh)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APGPhysicsCargoActor* CargoActor = World->SpawnActor<APGPhysicsCargoActor>(APGPhysicsCargoActor::StaticClass(), SpawnTransform, SpawnParameters);
	if (!CargoActor)
	{
		return nullptr;
	}

	CargoActor->InitializeCargo(CargoType, CachedCargoSettings.CargoCollisionProfileName);
	CargoActors.Add(CargoActor);
	return CargoActor;
}

UPGCargoTypeDefinition* UPGCargoStackComponent::ResolveDefaultCargoType() const
{
	return CachedCargoSettings.DefaultCargoType.LoadSynchronous();
}

FTransform UPGCargoStackComponent::MakeCargoWorldTransform(const FVector& LocalOffset, int32 CargoIndex) const
{
	const FVector StackedOffset = LocalOffset + FVector(0.0f, 0.0f, CargoIndex * CachedCargoSettings.InitialCargoSpacing);
	return FTransform(GetComponentQuat(), GetComponentTransform().TransformPosition(StackedOffset), FVector::OneVector);
}

void UPGCargoStackComponent::ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput)
{
	CargoShakeElapsedTime += DeltaSeconds;

	const FVector RightVector = GetRightVector();
	const float ShakePhase = CargoShakeElapsedTime * CachedCargoSettings.CargoIdleShakeFrequency * UE_TWO_PI;

	for (int32 CargoIndex = 0; CargoIndex < CargoActors.Num(); ++CargoIndex)
	{
		APGPhysicsCargoActor* CargoActor = CargoActors[CargoIndex];
		if (!CargoActor || !CargoActor->GetCargoMeshComponent())
		{
			continue;
		}

		// BP 바구니 충돌에만 의존하면 흔들림 체감이 약할 수 있어, 중심 입력 기반의 약한 보조 힘을 계란에 더한다.
		const float IdleShake = FMath::Sin(ShakePhase + CargoIndex * 1.73f) * CachedCargoSettings.CargoIdleShakeForce;
		const float BalanceForce = BalanceInput * CachedCargoSettings.CargoBalanceForce;
		CargoActor->GetCargoMeshComponent()->AddForce(RightVector * (IdleShake + BalanceForce), NAME_None, true);
	}
}

void UPGCargoStackComponent::CheckCargoLoss()
{
	const FVector AnchorLocation = GetComponentLocation();
	const float LossZ = AnchorLocation.Z - CachedCargoSettings.CargoLossZOffset;
	const float LossDistanceSquared = FMath::Square(CachedCargoSettings.CargoLossHorizontalDistance);

	for (int32 CargoIndex = CargoActors.Num() - 1; CargoIndex >= 0; --CargoIndex)
	{
		APGPhysicsCargoActor* CargoActor = CargoActors[CargoIndex];
		if (!CargoActor)
		{
			CargoActors.RemoveAt(CargoIndex);
			continue;
		}

		const FVector CargoLocation = CargoActor->GetActorLocation();
		const FVector CargoDelta = CargoLocation - AnchorLocation;
		const float HorizontalDistanceSquared = FVector(CargoDelta.X, CargoDelta.Y, 0.0f).SizeSquared();
		if (CargoLocation.Z < LossZ || HorizontalDistanceSquared > LossDistanceSquared)
		{
			LoseCargoAt(CargoIndex);
		}
	}
}

void UPGCargoStackComponent::LoseCargoAt(int32 CargoIndex)
{
	if (!CargoActors.IsValidIndex(CargoIndex))
	{
		return;
	}

	if (APGPhysicsCargoActor* CargoActor = CargoActors[CargoIndex])
	{
		CargoActor->Destroy();
	}

	CargoActors.RemoveAt(CargoIndex);
	OnCargoLost.Broadcast(CargoIndex);
}

void UPGCargoStackComponent::CleanupInvalidCargoActors()
{
	for (int32 CargoIndex = CargoActors.Num() - 1; CargoIndex >= 0; --CargoIndex)
	{
		if (!CargoActors[CargoIndex])
		{
			CargoActors.RemoveAt(CargoIndex);
		}
	}
}

void UPGCargoStackComponent::DrawCargoDebug() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (!CachedCargoSettings.bDrawCargoDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector AnchorLocation = GetComponentLocation();
	DrawDebugSphere(World, AnchorLocation, 12.0f, 12, FColor::Cyan, false, 0.0f, 0, 1.5f);
	DrawDebugCircle(World, AnchorLocation, CachedCargoSettings.CargoLossHorizontalDistance, 32, FColor::Red, false, 0.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugLine(World, AnchorLocation, AnchorLocation - FVector::UpVector * CachedCargoSettings.CargoLossZOffset, FColor::Red, false, 0.0f, 0, 1.0f);

	for (const APGPhysicsCargoActor* CargoActor : CargoActors)
	{
		if (CargoActor)
		{
			DrawDebugSphere(World, CargoActor->GetActorLocation(), 8.0f, 8, FColor::Yellow, false, 0.0f, 0, 1.0f);
		}
	}
#endif
}
