// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

#include "Cargo/PGCargoTypeDefinition.h"
#include "Cargo/PGPhysicsCargoActor.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGCargoStack, Log, All);

UPGCargoStackComponent::UPGCargoStackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ScriptedCargoMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScriptedCargoDisplay"));
	ScriptedCargoMeshComponent->SetupAttachment(this);
	ScriptedCargoMeshComponent->SetMobility(EComponentMobility::Movable);
	ScriptedCargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScriptedCargoMeshComponent->SetGenerateOverlapEvents(false);
	ScriptedCargoMeshComponent->SetVisibility(false, true);
	ScriptedCargoMeshComponent->SetHiddenInGame(true, true);
}

void UPGCargoStackComponent::InitializeCargoStack(const FPGCargoSettings& InCargoSettings)
{
	CachedCargoSettings = InCargoSettings;
	CargoActors.Reset();
	PendingCargoType = nullptr;
	PendingInitialCargoType = nullptr;
	bHasPendingCargoDrop = false;
	bIsInitialCargoLockActive = false;
	PendingInitialCargoCount = 0;
	InitialCargoLockRemainingTime = 0.0f;
	InitialCargoSettleRemainingTime = 0.0f;
	CargoDropHoldRemainingTime = 0.0f;
	CargoShakeElapsedTime = 0.0f;
	PreviousAnchorLocation = GetComponentLocation();
	AnchorVelocity = FVector::ZeroVector;
	LaneJumpCargoAssistRemainingTime = 0.0f;
	bWasChangingLaneLastFrame = false;
	bHasPreviousAnchorLocation = true;

	PreviousAnchorLocation = GetComponentLocation();
	SpawnInitialCargo();
}

void UPGCargoStackComponent::UpdateCargoReaction(float DeltaSeconds, float BalanceInput, bool bIsChangingLane)
{
	CleanupInvalidCargoActors();
	UpdateAnchorMotion(DeltaSeconds, bIsChangingLane);
	UpdateInitialCargoLock(DeltaSeconds);
	ApplyCargoPhysicsForces(DeltaSeconds, BalanceInput);
	ApplyLaneJumpCargoAssist();

	if (bHasPendingCargoDrop)
	{
		ApplyScriptedCargoDisplayTransform(PendingCargoType);
		CargoDropHoldRemainingTime = FMath::Max(0.0f, CargoDropHoldRemainingTime - DeltaSeconds);
		if (CargoDropHoldRemainingTime <= 0.0f)
		{
			FinishCargoDropHold();
		}
	}

	CheckCargoLoss();
	DrawCargoDebug();
}

bool UPGCargoStackComponent::AddCargo()
{
	return StartCargoDropHold(ResolveDefaultCargoType());
}

bool UPGCargoStackComponent::AddCargoByType(UPGCargoTypeDefinition* CargoType)
{
	return StartCargoDropHold(CargoType);
}

void UPGCargoStackComponent::SpawnInitialCargo()
{
	UPGCargoTypeDefinition* CargoType = ResolveDefaultCargoType();
	if (!IsValidCargoTypeForSpawn(CargoType, TEXT("SpawnInitialCargo")))
	{
		return;
	}

	const int32 InitialCount = FMath::Clamp(CachedCargoSettings.InitialCargoCount, 0, FMath::Max(1, CachedCargoSettings.MaxCargoCount));
	if (InitialCount <= 0)
	{
		return;
	}

	if (CachedCargoSettings.InitialIntroLockDuration > KINDA_SMALL_NUMBER)
	{
		StartInitialCargoLock(CargoType, InitialCount);
		return;
	}

	for (int32 CargoIndex = 0; CargoIndex < InitialCount; ++CargoIndex)
	{
		SpawnPhysicsCargo(CargoType, MakeCargoWorldTransform(CargoIndex));
	}
}

void UPGCargoStackComponent::StartInitialCargoLock(UPGCargoTypeDefinition* CargoType, int32 InitialCount)
{
	if (!IsValidCargoTypeForSpawn(CargoType, TEXT("StartInitialCargoLock")) || InitialCount <= 0)
	{
		return;
	}

	PendingInitialCargoType = CargoType;
	PendingInitialCargoCount = FMath::Clamp(InitialCount, 1, CachedCargoSettings.MaxCargoCount);
	bIsInitialCargoLockActive = true;
	InitialCargoLockRemainingTime = FMath::Max(0.0f, CachedCargoSettings.InitialIntroLockDuration);
	InitialCargoSettleRemainingTime = 0.0f;

	if (InitialCargoLockRemainingTime <= KINDA_SMALL_NUMBER)
	{
		FinishInitialCargoLock();
	}
}

void UPGCargoStackComponent::UpdateInitialCargoLock(float DeltaSeconds)
{
	if (bIsInitialCargoLockActive)
	{
		InitialCargoLockRemainingTime = FMath::Max(0.0f, InitialCargoLockRemainingTime - FMath::Max(0.0f, DeltaSeconds));
		if (InitialCargoLockRemainingTime <= 0.0f)
		{
			FinishInitialCargoLock();
		}
		return;
	}

	if (InitialCargoSettleRemainingTime > 0.0f)
	{
		InitialCargoSettleRemainingTime = FMath::Max(0.0f, InitialCargoSettleRemainingTime - DeltaSeconds);
	}
}

void UPGCargoStackComponent::FinishInitialCargoLock()
{
	if (!bIsInitialCargoLockActive)
	{
		return;
	}

	UPGCargoTypeDefinition* CargoType = PendingInitialCargoType;
	const int32 SpawnCount = PendingInitialCargoCount;

	bIsInitialCargoLockActive = false;
	PendingInitialCargoType = nullptr;
	PendingInitialCargoCount = 0;
	InitialCargoLockRemainingTime = 0.0f;
	InitialCargoSettleRemainingTime = FMath::Max(0.0f, CachedCargoSettings.InitialCargoSettleDelay);

	if (ScriptedCargoMeshComponent)
	{
		ScriptedCargoMeshComponent->SetVisibility(false, true);
		ScriptedCargoMeshComponent->SetHiddenInGame(true, true);
	}

	for (int32 CargoIndex = 0; CargoIndex < SpawnCount; ++CargoIndex)
	{
		SpawnPhysicsCargo(CargoType, MakeCargoWorldTransform(CargoIndex));
	}
}

bool UPGCargoStackComponent::StartCargoDropHold(UPGCargoTypeDefinition* CargoType)
{
	if (!IsValidCargoTypeForSpawn(CargoType, TEXT("StartCargoDropHold")))
	{
		return false;
	}

	if (bIsInitialCargoLockActive)
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
	CargoDropHoldRemainingTime = FMath::Max(0.0f, CachedCargoSettings.CargoDropHoldDuration);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (CargoDropHoldRemainingTime <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPGCargoStack, Warning, TEXT("CargoDrop Hold duration is %.3fs. Cargo will transition to physics immediately. Mesh=%s"),
			CachedCargoSettings.CargoDropHoldDuration,
			*GetNameSafe(CargoType->CargoMesh));
	}
#endif

	if (ScriptedCargoMeshComponent)
	{
		// 추가 적재물은 물리 전환 전까지 CargoStack 위치에서 대기해 플레이어가 다음 적재를 직관적으로 인지하게 한다.
		ScriptedCargoMeshComponent->SetStaticMesh(CargoType->CargoMesh);
		ApplyScriptedCargoDisplayTransform(CargoType);
		ScriptedCargoMeshComponent->SetVisibility(true, true);
		ScriptedCargoMeshComponent->SetHiddenInGame(false, true);
	}

	OnCargoDropHoldStarted.Broadcast();

	if (CargoDropHoldRemainingTime <= 0.0f)
	{
		FinishCargoDropHold();
	}

	return true;
}

void UPGCargoStackComponent::FinishCargoDropHold()
{
	if (!bHasPendingCargoDrop)
	{
		return;
	}

	if (CargoDropHoldRemainingTime > KINDA_SMALL_NUMBER)
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		UE_LOG(LogPGCargoStack, Warning, TEXT("FinishCargoDropHold was ignored because hold is still active. Remaining=%.3fs"), CargoDropHoldRemainingTime);
#endif
		return;
	}

	UPGCargoTypeDefinition* CargoType = PendingCargoType;
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoDropHoldRemainingTime = 0.0f;

	if (ScriptedCargoMeshComponent)
	{
		ScriptedCargoMeshComponent->SetVisibility(false, true);
		ScriptedCargoMeshComponent->SetHiddenInGame(true, true);
	}

	// 대기 메시와 실제 물리 전환은 모두 CargoStackComponent 위치를 사용합니다.
	SpawnPhysicsCargo(CargoType, MakeCargoWorldTransform());
	OnCargoDropHoldFinished.Broadcast();
}

APGPhysicsCargoActor* UPGCargoStackComponent::SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform)
{
	if (!IsValidCargoTypeForSpawn(CargoType, TEXT("SpawnPhysicsCargo")))
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
	if (UStaticMeshComponent* CargoMeshComponent = CargoActor->GetCargoMeshComponent())
	{
		// 물리 적재물은 부착하지 않지만, 생성 순간에는 앵커 속도를 상속해 자동 전진 중 뒤처지지 않게 합니다.
		CargoMeshComponent->SetPhysicsLinearVelocity(AnchorVelocity);
	}

	CargoActors.Add(CargoActor);
	return CargoActor;
}

UPGCargoTypeDefinition* UPGCargoStackComponent::ResolveDefaultCargoType() const
{
	return CachedCargoSettings.DefaultCargoType.LoadSynchronous();
}

bool UPGCargoStackComponent::IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const
{
	if (!CargoType)
	{
		UE_LOG(LogPGCargoStack, Warning, TEXT("%s failed: DefaultCargoType is not set or could not be loaded."), Context);
		return false;
	}

	if (!CargoType->CargoMesh)
	{
		UE_LOG(LogPGCargoStack, Warning, TEXT("%s failed: CargoType '%s' has no CargoMesh."), Context, *GetNameSafe(CargoType));
		return false;
	}

	return true;
}

FTransform UPGCargoStackComponent::MakeCargoWorldTransform(int32 CargoIndex) const
{
	const FVector StackedOffset = FVector(0.0f, 0.0f, CargoIndex * CachedCargoSettings.InitialCargoSpacing);
	return FTransform(GetComponentQuat(), GetComponentTransform().TransformPosition(StackedOffset), FVector::OneVector);
}

void UPGCargoStackComponent::ApplyScriptedCargoDisplayTransform(const UPGCargoTypeDefinition* CargoType)
{
	if (!ScriptedCargoMeshComponent || !CargoType)
	{
		return;
	}

	const FTransform SpawnTransform = MakeCargoWorldTransform();
	const FQuat DisplayRotation = SpawnTransform.GetRotation() * CargoType->MeshRotation.Quaternion();

	ScriptedCargoMeshComponent->SetWorldLocationAndRotation(SpawnTransform.GetLocation(), DisplayRotation);
	ScriptedCargoMeshComponent->SetWorldScale3D(CargoType->MeshScale);
}

FVector UPGCargoStackComponent::GetLossOriginLocation() const
{
	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetActorLocation();
	}

	return GetComponentLocation();
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

		// BP 받침 충돌에만 의존하면 흔들림 체감이 약할 수 있어, 중심 입력 기반의 약한 보조 힘을 적재물에 더한다.
		const float IdleShake = FMath::Sin(ShakePhase + CargoIndex * 1.73f) * CachedCargoSettings.CargoIdleShakeForce;
		const float BalanceForce = BalanceInput * CachedCargoSettings.CargoBalanceForce;
		CargoActor->GetCargoMeshComponent()->AddForce(RightVector * (IdleShake + BalanceForce), NAME_None, true);
	}
}

void UPGCargoStackComponent::UpdateAnchorMotion(float DeltaSeconds, bool bIsChangingLane)
{
	const FVector CurrentAnchorLocation = GetComponentLocation();
	if (bHasPreviousAnchorLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		AnchorVelocity = (CurrentAnchorLocation - PreviousAnchorLocation) / DeltaSeconds;
	}
	else
	{
		AnchorVelocity = FVector::ZeroVector;
		bHasPreviousAnchorLocation = true;
	}

	PreviousAnchorLocation = CurrentAnchorLocation;

	if (bIsChangingLane || (bWasChangingLaneLastFrame && !bIsChangingLane))
	{
		LaneJumpCargoAssistRemainingTime = FMath::Max(0.0f, CachedCargoSettings.LaneJumpCargoAssistDuration);
	}
	else if (LaneJumpCargoAssistRemainingTime > 0.0f)
	{
		LaneJumpCargoAssistRemainingTime = FMath::Max(0.0f, LaneJumpCargoAssistRemainingTime - DeltaSeconds);
	}

	bWasChangingLaneLastFrame = bIsChangingLane;
}

void UPGCargoStackComponent::ApplyLaneJumpCargoAssist()
{
	if (LaneJumpCargoAssistRemainingTime <= 0.0f)
	{
		return;
	}

	const float MaxRelativeSpeed = FMath::Max(0.0f, CachedCargoSettings.LaneJumpMaxRelativeSpeed);
	const float MaxUpwardRelativeSpeed = FMath::Max(0.0f, CachedCargoSettings.LaneJumpMaxUpwardRelativeSpeed);
	if (MaxRelativeSpeed <= KINDA_SMALL_NUMBER && MaxUpwardRelativeSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (APGPhysicsCargoActor* CargoActor : CargoActors)
	{
		if (!CargoActor || !CargoActor->GetCargoMeshComponent())
		{
			continue;
		}

		UStaticMeshComponent* CargoMeshComponent = CargoActor->GetCargoMeshComponent();
		FVector RelativeVelocity = CargoMeshComponent->GetPhysicsLinearVelocity() - AnchorVelocity;

		if (MaxRelativeSpeed > KINDA_SMALL_NUMBER && RelativeVelocity.SizeSquared() > FMath::Square(MaxRelativeSpeed))
		{
			RelativeVelocity = RelativeVelocity.GetClampedToMaxSize(MaxRelativeSpeed);
		}

		if (RelativeVelocity.Z > MaxUpwardRelativeSpeed)
		{
			// 라인 점프 중 받침이 급격히 움직여 적재물이 위로 튕겨나가는 성분만 제한한다.
			RelativeVelocity.Z = MaxUpwardRelativeSpeed;
		}

		CargoMeshComponent->SetPhysicsLinearVelocity(AnchorVelocity + RelativeVelocity);
	}
}

void UPGCargoStackComponent::CheckCargoLoss()
{
	if (bIsInitialCargoLockActive || InitialCargoSettleRemainingTime > 0.0f)
	{
		return;
	}

	const FVector LossOriginLocation = GetLossOriginLocation();
	const float LossZ = LossOriginLocation.Z - CachedCargoSettings.CargoLossZOffset;
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
		const FVector CargoDelta = CargoLocation - LossOriginLocation;
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

	const FVector CargoAnchorLocation = GetComponentLocation();
	const FVector LossOriginLocation = GetLossOriginLocation();
	DrawDebugSphere(World, CargoAnchorLocation, 12.0f, 12, FColor::Cyan, false, 0.0f, 0, 1.5f);
	DrawDebugCircle(World, LossOriginLocation, CachedCargoSettings.CargoLossHorizontalDistance, 32, FColor::Red, false, 0.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugLine(World, LossOriginLocation, LossOriginLocation - FVector::UpVector * CachedCargoSettings.CargoLossZOffset, FColor::Red, false, 0.0f, 0, 1.0f);

	if (bHasPendingCargoDrop)
	{
		const FVector HoldLocation = GetComponentLocation();
		DrawDebugSphere(World, HoldLocation, 18.0f, 16, FColor::Purple, false, 0.0f, 0, 2.0f);
	}

	for (const APGPhysicsCargoActor* CargoActor : CargoActors)
	{
		if (CargoActor)
		{
			DrawDebugSphere(World, CargoActor->GetActorLocation(), 8.0f, 8, FColor::Yellow, false, 0.0f, 0, 1.0f);
		}
	}
#endif
}
