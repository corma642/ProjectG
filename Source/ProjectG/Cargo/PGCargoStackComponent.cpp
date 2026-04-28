// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

#include "Cargo/PGCargoTypeDefinition.h"
#include "Cargo/PGPhysicsCargoActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

UPGCargoStackComponent::UPGCargoStackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	BasketMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BasketMesh"));
	BasketMeshComponent->SetupAttachment(this);
	BasketMeshComponent->SetMobility(EComponentMobility::Movable);
	BasketMeshComponent->SetSimulatePhysics(false);
	BasketMeshComponent->SetGenerateOverlapEvents(false);

	PreviewCargoMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoPreview"));
	PreviewCargoMeshComponent->SetupAttachment(this);
	PreviewCargoMeshComponent->SetMobility(EComponentMobility::Movable);
	PreviewCargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewCargoMeshComponent->SetGenerateOverlapEvents(false);
	PreviewCargoMeshComponent->SetVisibility(false, true);
	PreviewCargoMeshComponent->SetHiddenInGame(true, true);
}

void UPGCargoStackComponent::InitializeCargoStack(const FPGCargoSettings& InCargoSettings, USkeletalMeshComponent* RunnerMesh)
{
	CachedCargoSettings = InCargoSettings;
	CargoActors.Reset();
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoPreviewRemainingTime = 0.0f;
	CurrentBasketRoll = 0.0f;
	CargoShakeElapsedTime = 0.0f;

	AttachBasketRoot(RunnerMesh);
	ApplyBasketSettings();
	SpawnInitialCargo();
}

void UPGCargoStackComponent::UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane)
{
	CleanupInvalidCargoActors();
	UpdateBasketTransform(DeltaSeconds, BalanceInput, VisualJumpOffset, bIsChangingLane);
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

void UPGCargoStackComponent::AttachBasketRoot(USkeletalMeshComponent* RunnerMesh)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FName SocketName = CachedCargoSettings.BasketSocketName;
	if (RunnerMesh && !SocketName.IsNone() && RunnerMesh->DoesSocketExist(SocketName))
	{
		// 머리 소켓이 있으면 바구니 기준 컴포넌트 자체를 소켓에 붙여 캐릭터 애니메이션을 따라가게 한다.
		AttachToComponent(RunnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
		SetRelativeLocation(FVector::ZeroVector);
		SetRelativeRotation(FRotator::ZeroRotator);
		return;
	}

	if (USceneComponent* OwnerRoot = Owner->GetRootComponent())
	{
		// 소켓이 아직 준비되지 않은 에디터 테스트에서는 캡슐 루트 기준 fallback 위치를 사용한다.
		AttachToComponent(OwnerRoot, FAttachmentTransformRules::KeepRelativeTransform);
		SetRelativeLocation(CachedCargoSettings.BasketFallbackOffset);
		SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void UPGCargoStackComponent::ApplyBasketSettings()
{
	if (!BasketMeshComponent)
	{
		return;
	}

	BasketMeshComponent->SetStaticMesh(CachedCargoSettings.BasketMesh.LoadSynchronous());
	BasketMeshComponent->SetRelativeLocation(CachedCargoSettings.BasketRelativeLocation);
	BasketMeshComponent->SetRelativeRotation(CachedCargoSettings.BasketRelativeRotation);
	BasketMeshComponent->SetRelativeScale3D(CachedCargoSettings.BasketRelativeScale);
	BasketMeshComponent->SetSimulatePhysics(false);
	BasketMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BasketMeshComponent->SetCollisionProfileName(CachedCargoSettings.BasketCollisionProfileName);
	BasketMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	BasketMeshComponent->SetGenerateOverlapEvents(false);
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
		// 프리뷰는 충돌/물리 없이 위치만 보여줘서 플레이어가 곧 떨어질 계란을 예측하게 한다.
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

void UPGCargoStackComponent::UpdateBasketTransform(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane)
{
	if (!BasketMeshComponent)
	{
		return;
	}

	const float TargetRoll = BalanceInput * CachedCargoSettings.BasketSwayMaxRoll;
	CurrentBasketRoll = FMath::FInterpTo(CurrentBasketRoll, TargetRoll, DeltaSeconds, CachedCargoSettings.BasketSwayInterpSpeed);

	const float TargetJumpLift = bIsChangingLane ? VisualJumpOffset * CachedCargoSettings.BasketJumpLiftScale : 0.0f;
	const FVector BasketLocation = CachedCargoSettings.BasketRelativeLocation + FVector(0.0f, 0.0f, TargetJumpLift);
	const FQuat BaseRotation = CachedCargoSettings.BasketRelativeRotation.Quaternion();
	const FQuat SwayRotation(FVector::ForwardVector, FMath::DegreesToRadians(CurrentBasketRoll));

	BasketMeshComponent->SetRelativeLocation(BasketLocation);
	BasketMeshComponent->SetRelativeRotation((SwayRotation * BaseRotation).Rotator());
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

		// 바구니 회전만으로 부족한 물리 흔들림을 보강한다. AccelChange를 사용해 계란 질량 차이의 영향을 줄인다.
		const float IdleShake = FMath::Sin(ShakePhase + CargoIndex * 1.73f) * CachedCargoSettings.CargoIdleShakeForce;
		const float BalanceForce = BalanceInput * CachedCargoSettings.CargoBalanceForce;
		CargoActor->GetCargoMeshComponent()->AddForce(RightVector * (IdleShake + BalanceForce), NAME_None, true);
	}
}

void UPGCargoStackComponent::CheckCargoLoss()
{
	const FVector BasketLocation = BasketMeshComponent ? BasketMeshComponent->GetComponentLocation() : GetComponentLocation();
	const float LossZ = BasketLocation.Z - CachedCargoSettings.CargoLossZOffset;
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
		const FVector CargoDelta = CargoLocation - BasketLocation;
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

	const FVector BasketLocation = BasketMeshComponent ? BasketMeshComponent->GetComponentLocation() : GetComponentLocation();
	DrawDebugSphere(World, BasketLocation, 12.0f, 12, FColor::Cyan, false, 0.0f, 0, 1.5f);
	DrawDebugCircle(World, BasketLocation, CachedCargoSettings.CargoLossHorizontalDistance, 32, FColor::Red, false, 0.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugLine(World, BasketLocation, BasketLocation - FVector::UpVector * CachedCargoSettings.CargoLossZOffset, FColor::Red, false, 0.0f, 0, 1.0f);

	for (const APGPhysicsCargoActor* CargoActor : CargoActors)
	{
		if (CargoActor)
		{
			DrawDebugSphere(World, CargoActor->GetActorLocation(), 8.0f, 8, FColor::Yellow, false, 0.0f, 0, 1.0f);
		}
	}
#endif
}
