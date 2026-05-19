// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

#include "Audio/PGAudioStatics.h"
#include "Cargo/PGCargoTypeDefinition.h"
#include "Cargo/PGPhysicsCargoActor.h"
#include "Components/StaticMeshComponent.h"
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
	CargoInstances.Reset();
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoDropHoldRemainingTime = 0.0f;
	CargoShakeElapsedTime = 0.0f;
	PreviousAnchorLocation = GetComponentLocation();
	AnchorVelocity = FVector::ZeroVector;
	AirborneCargoAssistRemainingTime = 0.0f;
	bWasAirborneLastFrame = false;
	bHasPreviousAnchorLocation = true;
	NextCargoInstanceId = 1;
	DeliveredCargoCount = 0;

	if (ScriptedCargoMeshComponent)
	{
		ScriptedCargoMeshComponent->SetStaticMesh(nullptr);
		ScriptedCargoMeshComponent->SetVisibility(false, true);
		ScriptedCargoMeshComponent->SetHiddenInGame(true, true);
	}

	OnCargoListChanged.Broadcast();
}

void UPGCargoStackComponent::UpdateCargoReaction(float DeltaSeconds, float BalanceInput, bool bIsAirborne)
{
	CleanupInvalidCargoActors();
	UpdateAnchorMotion(DeltaSeconds, bIsAirborne);
	ApplyCargoPhysicsForces(DeltaSeconds, BalanceInput);
	ApplyAirborneCargoAssist();

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
	UpdateCargoDeliveries(DeltaSeconds);
}

bool UPGCargoStackComponent::AddCargoByType(UPGCargoTypeDefinition* CargoType)
{
	return StartCargoDropHold(CargoType);
}

TArray<FPGActiveCargoInfo> UPGCargoStackComponent::GetActiveCargoInfos() const
{
	TArray<FPGActiveCargoInfo> Infos;
	Infos.Reserve(CargoInstances.Num());

	for (const FPGRuntimeCargoInstance& CargoInstance : CargoInstances)
	{
		if (!CargoInstance.CargoActor || !CargoInstance.CargoType)
		{
			continue;
		}

		const float DeliveryDuration = FMath::Max(0.0f, CargoInstance.CargoType->DeliveryDuration);
		FPGActiveCargoInfo Info;
		Info.InstanceId = CargoInstance.InstanceId;
		Info.CargoType = CargoInstance.CargoType;
		Info.CargoThumbnail = CargoInstance.CargoType->CargoThumbnail;
		Info.DeliveryDuration = DeliveryDuration;
		Info.RemainingDeliveryTime = FMath::Max(0.0f, DeliveryDuration - CargoInstance.DeliveryElapsedTime);
		Info.DeliveryProgress = DeliveryDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(CargoInstance.DeliveryElapsedTime / DeliveryDuration, 0.0f, 1.0f)
			: 1.0f;
		Infos.Add(Info);
	}

	return Infos;
}

bool UPGCargoStackComponent::StartCargoDropHold(UPGCargoTypeDefinition* CargoType)
{
	if (!IsValidCargoTypeForSpawn(CargoType, TEXT("StartCargoDropHold")))
	{
		return false;
	}

	if (bHasPendingCargoDrop)
	{
		return false;
	}

	PendingCargoType = CargoType;
	bHasPendingCargoDrop = true;
	CargoDropHoldRemainingTime = FMath::Max(0.0f, CachedCargoSettings.CargoDropHoldDuration);

	if (ScriptedCargoMeshComponent)
	{
		// 물리 전환 전까지 실제 스폰 위치와 같은 Transform으로 Hold 메시를 보여줘 즉시 생성처럼 보이지 않게 한다.
		ScriptedCargoMeshComponent->SetStaticMesh(CargoType->CargoMesh);
		ApplyScriptedCargoDisplayTransform(CargoType);
		ScriptedCargoMeshComponent->SetVisibility(true, true);
		ScriptedCargoMeshComponent->SetHiddenInGame(false, true);
	}

	OnCargoListChanged.Broadcast();

	if (CargoDropHoldRemainingTime <= 0.0f)
	{
		// Hold 시간이 0이면 같은 프레임에 바로 물리 적재물로 전환한다.
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
		return;
	}

	UPGCargoTypeDefinition* CargoType = PendingCargoType;
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoDropHoldRemainingTime = 0.0f;

	if (ScriptedCargoMeshComponent)
	{
		// 프리뷰 메시를 먼저 숨기고 물리 Actor를 생성해, 같은 위치에서 자연스럽게 전환되는 느낌을 만든다.
		ScriptedCargoMeshComponent->SetVisibility(false, true);
		ScriptedCargoMeshComponent->SetHiddenInGame(true, true);
	}

	APGPhysicsCargoActor* SpawnedCargoActor = SpawnPhysicsCargo(CargoType, MakeCargoWorldTransform());
	if (SpawnedCargoActor)
	{
		if (const UPGGameSettings* Settings = GetDefault<UPGGameSettings>())
		{
			UPGAudioStatics::PlaySFXAtLocation(this, Settings->SFX.CargoDropSound.Get(), SpawnedCargoActor->GetActorLocation());
		}
	}
	OnCargoListChanged.Broadcast();
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
		// 생성 순간 CargoStack 앵커 속도를 상속해 자동 전진 중에도 적재물이 뒤로 처지지 않게 한다.
		CargoMeshComponent->SetPhysicsLinearVelocity(AnchorVelocity);
	}

	FPGRuntimeCargoInstance CargoInstance;
	CargoInstance.CargoActor = CargoActor;
	CargoInstance.CargoType = CargoType;
	CargoInstance.InstanceId = NextCargoInstanceId++;
	CargoInstance.DeliveryElapsedTime = 0.0f;
	CargoInstances.Add(CargoInstance);
	OnCargoListChanged.Broadcast();
	return CargoActor;
}

bool UPGCargoStackComponent::IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const
{
	if (!CargoType)
	{
		UE_LOG(LogPGCargoStack, Warning, TEXT("%s failed: CargoType is not set."), Context);
		return false;
	}

	if (!CargoType->CargoMesh)
	{
		UE_LOG(LogPGCargoStack, Warning, TEXT("%s failed: CargoType '%s' has no CargoMesh."), Context, *GetNameSafe(CargoType));
		return false;
	}

	return true;
}

FTransform UPGCargoStackComponent::MakeCargoWorldTransform() const
{
	return FTransform(GetComponentQuat(), GetComponentLocation(), FVector::OneVector);
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

	for (int32 CargoIndex = 0; CargoIndex < CargoInstances.Num(); ++CargoIndex)
	{
		APGPhysicsCargoActor* CargoActor = CargoInstances[CargoIndex].CargoActor;
		if (!CargoActor || !CargoActor->GetCargoMeshComponent())
		{
			continue;
		}

		// 받침 충돌만으로는 흔들림 체감이 약하므로, 기본 흔들림과 플레이어 중심 입력을 보조 힘으로 더한다.
		const float IdleShake = FMath::Sin(ShakePhase + CargoIndex * 1.73f) * CachedCargoSettings.CargoIdleShakeForce;
		const float BalanceForce = BalanceInput * CachedCargoSettings.CargoBalanceForce;
		CargoActor->GetCargoMeshComponent()->AddForce(RightVector * (IdleShake + BalanceForce), NAME_None, true);
	}
}

void UPGCargoStackComponent::UpdateCargoDeliveries(float DeltaSeconds)
{
	for (int32 CargoIndex = CargoInstances.Num() - 1; CargoIndex >= 0; --CargoIndex)
	{
		FPGRuntimeCargoInstance& CargoInstance = CargoInstances[CargoIndex];
		if (!CargoInstance.CargoActor || !CargoInstance.CargoType)
		{
			continue;
		}

		CargoInstance.DeliveryElapsedTime += DeltaSeconds;
		const float DeliveryDuration = FMath::Max(0.0f, CargoInstance.CargoType->DeliveryDuration);
		if (CargoInstance.DeliveryElapsedTime >= DeliveryDuration)
		{
			CompleteCargoAt(CargoIndex);
		}
	}
}

void UPGCargoStackComponent::UpdateAnchorMotion(float DeltaSeconds, bool bIsAirborne)
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

	if (bIsAirborne || (bWasAirborneLastFrame && !bIsAirborne))
	{
		// 점프뿐 아니라 트랙 높이 차이로 잠깐 Falling이 된 경우도 적재물이 관성으로 튀지 않도록 짧게 보정한다.
		AirborneCargoAssistRemainingTime = FMath::Max(0.0f, CachedCargoSettings.AirborneCargoAssistDuration);
	}
	else if (AirborneCargoAssistRemainingTime > 0.0f)
	{
		AirborneCargoAssistRemainingTime = FMath::Max(0.0f, AirborneCargoAssistRemainingTime - DeltaSeconds);
	}

	bWasAirborneLastFrame = bIsAirborne;
}

void UPGCargoStackComponent::ApplyAirborneCargoAssist()
{
	if (AirborneCargoAssistRemainingTime <= 0.0f)
	{
		return;
	}

	const float MaxRelativeSpeed = FMath::Max(0.0f, CachedCargoSettings.AirborneCargoMaxRelativeSpeed);
	const float MaxUpwardRelativeSpeed = FMath::Max(0.0f, CachedCargoSettings.AirborneCargoMaxUpwardRelativeSpeed);
	if (MaxRelativeSpeed <= KINDA_SMALL_NUMBER && MaxUpwardRelativeSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (const FPGRuntimeCargoInstance& CargoInstance : CargoInstances)
	{
		APGPhysicsCargoActor* CargoActor = CargoInstance.CargoActor;
		if (!CargoActor || !CargoActor->GetCargoMeshComponent())
		{
			continue;
		}

		UStaticMeshComponent* CargoMeshComponent = CargoActor->GetCargoMeshComponent();
		FVector RelativeVelocity = CargoMeshComponent->GetPhysicsLinearVelocity() - AnchorVelocity;

		// 공중 보정은 앵커 기준 상대속도만 제한한다. 월드 속도를 직접 0으로 만들면 자동 전진과 충돌한다.
		if (MaxRelativeSpeed > KINDA_SMALL_NUMBER && RelativeVelocity.SizeSquared() > FMath::Square(MaxRelativeSpeed))
		{
			RelativeVelocity = RelativeVelocity.GetClampedToMaxSize(MaxRelativeSpeed);
		}

		if (RelativeVelocity.Z > MaxUpwardRelativeSpeed)
		{
			RelativeVelocity.Z = MaxUpwardRelativeSpeed;
		}

		CargoMeshComponent->SetPhysicsLinearVelocity(AnchorVelocity + RelativeVelocity);
	}
}

void UPGCargoStackComponent::CheckCargoLoss()
{
	const FVector LossOriginLocation = GetLossOriginLocation();
	const float LossZ = LossOriginLocation.Z - CachedCargoSettings.CargoLossZOffset;
	const float LossDistanceSquared = FMath::Square(CachedCargoSettings.CargoLossHorizontalDistance);

	// 손실 기준은 CargoStack 위치가 아니라 플레이어 Actor 위치다. CargoStack을 높게 배치해도 즉시 실패하지 않게 하기 위함이다.
	for (int32 CargoIndex = CargoInstances.Num() - 1; CargoIndex >= 0; --CargoIndex)
	{
		APGPhysicsCargoActor* CargoActor = CargoInstances[CargoIndex].CargoActor;
		if (!CargoActor)
		{
			CargoInstances.RemoveAt(CargoIndex);
			OnCargoListChanged.Broadcast();
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
	if (!CargoInstances.IsValidIndex(CargoIndex))
	{
		return;
	}

	if (APGPhysicsCargoActor* CargoActor = CargoInstances[CargoIndex].CargoActor)
	{
		CargoActor->Destroy();
	}

	CargoInstances.RemoveAt(CargoIndex);
	OnCargoListChanged.Broadcast();
	OnCargoLost.Broadcast(CargoIndex);
}

void UPGCargoStackComponent::CompleteCargoAt(int32 CargoIndex)
{
	if (!CargoInstances.IsValidIndex(CargoIndex))
	{
		return;
	}

	const FPGRuntimeCargoInstance CargoInstance = CargoInstances[CargoIndex];
	const int32 DeliveryScore = CargoInstance.CargoType ? FMath::Max(0, CargoInstance.CargoType->DeliveryScore) : 0;
	const int32 DeliveryReceipts = CargoInstance.CargoType ? FMath::Max(0, CargoInstance.CargoType->DeliveryReceipts) : 0;
	const int32 InstanceId = CargoInstance.InstanceId;
	const FVector DeliveryLocation = CargoInstance.CargoActor ? CargoInstance.CargoActor->GetActorLocation() : GetComponentLocation();

	// HUD가 완료 카드 위치를 찾을 수 있도록 Actor 제거와 리스트 변경 브로드캐스트보다 먼저 완료 이벤트를 보낸다.
	++DeliveredCargoCount;
	OnCargoDelivered.Broadcast(InstanceId, DeliveryScore, DeliveryReceipts, DeliveryLocation);

	if (CargoInstance.CargoActor)
	{
		CargoInstance.CargoActor->Destroy();
	}

	CargoInstances.RemoveAt(CargoIndex);
	OnCargoListChanged.Broadcast();
}

void UPGCargoStackComponent::CleanupInvalidCargoActors()
{
	for (int32 CargoIndex = CargoInstances.Num() - 1; CargoIndex >= 0; --CargoIndex)
	{
		if (!CargoInstances[CargoIndex].CargoActor)
		{
			CargoInstances.RemoveAt(CargoIndex);
			OnCargoListChanged.Broadcast();
		}
	}
}
