// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGCargoStackComponent.h"

#include "Audio/PGAudioStatics.h"
#include "Cargo/PGCargoTypeDefinition.h"
#include "Cargo/PGPhysicsCargoActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGCargoStack, Log, All);

namespace
{
	// 바구니 테두리보다 이만큼(cm) 위에 떠 있는 적재물은 아직 바구니로 떨어지는 중으로 본다. 공중 보정과 손실 임박 판정이 같은 기준을 쓴다.
	constexpr float SettledHeightAboveRim = 40.0f;
}

UPGCargoStackComponent::UPGCargoStackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPGCargoStackComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureScriptedCargoMeshComponent();
}

void UPGCargoStackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyScriptedCargoMeshComponent();
	Super::EndPlay(EndPlayReason);
}

void UPGCargoStackComponent::EnsureScriptedCargoMeshComponent()
{
	if (ScriptedCargoMeshComponent)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// CargoStack은 러너 BP의 기본 컴포넌트다. 생성자에서 자식 기본 서브오브젝트를 만들면
	// archetype과 런타임 인스턴스가 섞일 수 있으므로 Hold 표시는 러너 인스턴스에만 만든다.
	ScriptedCargoMeshComponent = NewObject<UStaticMeshComponent>(OwnerActor, TEXT("ScriptedCargoDisplay"), RF_Transient);
	OwnerActor->AddInstanceComponent(ScriptedCargoMeshComponent);
	ScriptedCargoMeshComponent->SetupAttachment(this);
	ScriptedCargoMeshComponent->SetMobility(EComponentMobility::Movable);
	ScriptedCargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScriptedCargoMeshComponent->SetGenerateOverlapEvents(false);
	ScriptedCargoMeshComponent->SetVisibility(false, true);
	ScriptedCargoMeshComponent->SetHiddenInGame(true, true);
	ScriptedCargoMeshComponent->RegisterComponent();
}

void UPGCargoStackComponent::DestroyScriptedCargoMeshComponent()
{
	if (ScriptedCargoMeshComponent)
	{
		ScriptedCargoMeshComponent->DestroyComponent();
		ScriptedCargoMeshComponent = nullptr;
	}
}

void UPGCargoStackComponent::InitializeCargoStack(const FPGCargoSettings& InCargoSettings)
{
	EnsureScriptedCargoMeshComponent();
	CachedCargoSettings = InCargoSettings;
	CargoInstances.Reset();
	PendingCargoType = nullptr;
	bHasPendingCargoDrop = false;
	CargoDropHoldRemainingTime = 0.0f;
	CargoShakeElapsedTime = 0.0f;
	PreviousAnchorLocation = GetComponentLocation();
	AnchorVelocity = FVector::ZeroVector;
	AirborneCargoAssistRemainingTime = 0.0f;
	CargoLossWorldZ = 0.0f;
	CargoDangerRatio = 0.0f;
	CargoDangerDirection = FVector2D::ZeroVector;
	bWasAirborneLastFrame = false;
	bHasPreviousAnchorLocation = true;
	bHasCargoLossWorldZ = false;
	bWarnedMissingCargoLossWorldZ = false;
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

void UPGCargoStackComponent::SetSupportComponent(const USceneComponent* InSupportComponent)
{
	SupportComponent = InSupportComponent;

	// 바구니가 바뀐 프레임에는 비교할 지난 자세가 없으므로 다음 갱신에서 속도를 다시 잡는다.
	bHasLatestSupportTransform = false;
}

void UPGCargoStackComponent::SetCargoLossWorldZ(float InCargoLossWorldZ)
{
	CargoLossWorldZ = InCargoLossWorldZ;
	bHasCargoLossWorldZ = true;
	bWarnedMissingCargoLossWorldZ = false;
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
			FinishCargoDropHold(DeltaSeconds);
		}
	}

	// 손실이 나면 같은 프레임에 런이 실패하고 보상이 정산된다. 이후 배달 완료가 점수를 더하면 정산 뒤 점수가 바뀌므로 건너뛴다.
	if (CheckCargoLoss(DeltaSeconds))
	{
		return;
	}
	UpdateCargoDeliveries(DeltaSeconds);
}

bool UPGCargoStackComponent::AddCargoByType(UPGCargoTypeDefinition* CargoType)
{
	return StartCargoDropHold(CargoType);
}

void UPGCargoStackComponent::GetActiveCargoInfos(TArray<FPGActiveCargoInfo>& OutInfos) const
{
	// HUD가 짧은 주기로 호출하므로 호출자가 가진 배열을 재사용해 매번 새 할당을 만들지 않는다.
	OutInfos.Reset(CargoInstances.Num());

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
		Info.DangerRatio = CargoInstance.DangerRatio;
		OutInfos.Add(Info);
	}
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
		FinishCargoDropHold(0.0f);
	}

	return true;
}

void UPGCargoStackComponent::FinishCargoDropHold(float PendingPhysicsSeconds)
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

	// 러너 Tick에서 만든 적재물은 같은 프레임 물리에서 앵커 속도로 한 프레임만큼 더 나아가지만, 바구니는 이번 프레임 위치에 그대로 있다.
	// 그만큼 뒤에서 만들어야 물리가 끝난 화면에서 Hold 위치와 겹치고, 착지 위치가 프레임 시간에 비례해 앞으로 밀리지 않는다.
	FTransform SpawnTransform = MakeCargoWorldTransform();
	SpawnTransform.AddToTranslation(-AnchorVelocity * PendingPhysicsSeconds);
	APGPhysicsCargoActor* SpawnedCargoActor = SpawnPhysicsCargo(CargoType, SpawnTransform);
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

	// 바구니는 머리 소켓에 붙어 있어 점프 직후 애니메이션만큼 캡슐보다 초속 1~2m 빠르게 솟고 착지 때는 더 깊이 꺼진다.
	// 담긴 적재물의 속도를 캡슐 기준으로 묶으면 바구니가 적재물을 뚫고 지나가므로 바구니 자체의 이동 속도를 따로 구한다.
	SupportVelocity = AnchorVelocity;
	if (const USceneComponent* Support = SupportComponent.Get())
	{
		// 적재물 위치는 지난 물리 스텝의 결과다. 바구니 안에 있는지는 그때의 바구니 자세와 비교해야 하므로 이번 프레임 자세로 바꾸기 전에 보관한다.
		const FTransform CurrentSupportTransform = Support->GetComponentTransform();
		SupportTransformAtLastStep = bHasLatestSupportTransform ? LatestSupportTransform : CurrentSupportTransform;
		LatestSupportTransform = CurrentSupportTransform;
		if (bHasLatestSupportTransform && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			SupportVelocity = (LatestSupportTransform.GetLocation() - SupportTransformAtLastStep.GetLocation()) / DeltaSeconds;
		}
		bHasLatestSupportTransform = true;
	}

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

	// 바구니에 담긴 적재물은 바구니 속도에, 아직 위에서 떨어지는 중인 적재물은 기존처럼 캡슐(앵커) 속도에 묶는다.
	// 바구니 밖으로 나간 적재물까지 묶으면 떨어지지 않고 공중에 뜬 채 러너를 따라오므로 보정하지 않는다.
	// 바구니 루트가 크기 없는 컴포넌트로 바뀌면 범위가 0이라 모든 적재물이 밖으로 판정된다. 그때는 범위를 따지지 않고 전부 보정한다.
	FBoxSphereBounds SupportBounds(ForceInit);
	float RimRadius = 0.0f;
	const bool bCheckFootprint = GetSupportFootprint(SupportBounds, RimRadius);

	for (const FPGRuntimeCargoInstance& CargoInstance : CargoInstances)
	{
		APGPhysicsCargoActor* CargoActor = CargoInstance.CargoActor;
		if (!CargoActor || !CargoActor->GetCargoMeshComponent())
		{
			continue;
		}

		UStaticMeshComponent* CargoMeshComponent = CargoActor->GetCargoMeshComponent();
		FVector ReferenceVelocity = AnchorVelocity;
		if (bCheckFootprint)
		{
			const FVector LocalOffset = GetCargoOffsetInSupport(*CargoMeshComponent, SupportBounds);

			// 테두리보다 한참 위에 떠 있는 적재물은 아직 바구니로 떨어지는 중이다. 바구니의 좌우 흔들림 속도까지 물려주면 풀려날 때 옆으로 던져지므로 캡슐 속도에 둔다.
			if (LocalOffset.Z < SupportBounds.BoxExtent.Z + SettledHeightAboveRim)
			{
				// 바구니 기준 좌표에서 테두리 반지름 밖이거나 바닥보다 아래면 이미 바구니를 벗어난 것이다.
				if (FVector2D(LocalOffset).SizeSquared() > FMath::Square(RimRadius) || LocalOffset.Z < -SupportBounds.BoxExtent.Z)
				{
					continue;
				}
				ReferenceVelocity = SupportVelocity;
			}
		}

		FVector RelativeVelocity = CargoMeshComponent->GetPhysicsLinearVelocity() - ReferenceVelocity;

		// 공중 보정은 앵커 기준 상대속도만 제한한다. 월드 속도를 직접 0으로 만들면 자동 전진과 충돌한다.
		if (MaxRelativeSpeed > KINDA_SMALL_NUMBER && RelativeVelocity.SizeSquared() > FMath::Square(MaxRelativeSpeed))
		{
			RelativeVelocity = RelativeVelocity.GetClampedToMaxSize(MaxRelativeSpeed);
		}

		if (RelativeVelocity.Z > MaxUpwardRelativeSpeed)
		{
			RelativeVelocity.Z = MaxUpwardRelativeSpeed;
		}

		CargoMeshComponent->SetPhysicsLinearVelocity(ReferenceVelocity + RelativeVelocity);
	}
}

bool UPGCargoStackComponent::GetSupportFootprint(FBoxSphereBounds& OutBounds, float& OutRimRadius) const
{
	const USceneComponent* Support = SupportComponent.Get();
	if (!Support)
	{
		return false;
	}

	OutBounds = Support->CalcBounds(FTransform(FQuat::Identity, FVector::ZeroVector, SupportTransformAtLastStep.GetScale3D()));
	OutRimRadius = FMath::Max(OutBounds.BoxExtent.X, OutBounds.BoxExtent.Y);
	return OutRimRadius > KINDA_SMALL_NUMBER;
}

FVector UPGCargoStackComponent::GetCargoOffsetInSupport(const UStaticMeshComponent& CargoMeshComponent, const FBoxSphereBounds& SupportBounds) const
{
	// 이번 프레임 자세와 비교하면 점프 첫 프레임에 바구니만 솟아 있어 멀쩡한 적재물이 바닥 아래로 판정된다. 지난 물리 스텝 자세를 쓴다.
	return SupportTransformAtLastStep.InverseTransformPositionNoScale(CargoMeshComponent.Bounds.Origin) - SupportBounds.Origin;
}

bool UPGCargoStackComponent::CheckCargoLoss(float DeltaSeconds)
{
	bool bAnyCargoLost = false;
	float MaxHorizontalDistanceSquared = 0.0f;
	float MaxRimRatio = 0.0f;
	FVector2D MaxRimOffset = FVector2D::ZeroVector;

	FBoxSphereBounds SupportBounds(ForceInit);
	float RimRadius = 0.0f;
	const bool bHasFootprint = GetSupportFootprint(SupportBounds, RimRadius);

	// 적재물 위치는 지난 물리 스텝의 결과인데 러너는 이미 이번 프레임만큼 나아갔다. 그대로 비교하면 바구니에 담긴 적재물도 한 프레임 이동 거리만큼 뒤처져 보여,
	// 긴 프레임 하나에 손실로 판정되고 손실 임박 경고가 프레임률에 따라 달라진다. 러너도 지난 프레임 위치로 되돌려 같은 시점끼리 비교한다.
	const FVector LossOriginLocation = GetLossOriginLocation() - AnchorVelocity * DeltaSeconds;
	const float LossDistanceSquared = FMath::Square(CachedCargoSettings.CargoLossHorizontalDistance);
	if (!bHasCargoLossWorldZ && !bWarnedMissingCargoLossWorldZ)
	{
		UE_LOG(LogPGCargoStack, Warning, TEXT("Cargo vertical loss check is disabled: the current map CargoLossWorldZ was not applied."));
		bWarnedMissingCargoLossWorldZ = true;
	}

	// 수평 손실은 러너 기준 거리를 유지하되, 수직 손실은 맵 DataAsset의 고정 월드 Z로 분리합니다.
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
		const bool bBelowLossWorldZ = bHasCargoLossWorldZ && CargoLocation.Z < CargoLossWorldZ;
		if (bBelowLossWorldZ || HorizontalDistanceSquared > LossDistanceSquared)
		{
			LoseCargoAt(CargoIndex);
			bAnyCargoLost = true;
			continue;
		}

		MaxHorizontalDistanceSquared = FMath::Max(MaxHorizontalDistanceSquared, HorizontalDistanceSquared);

		// 손실 임박은 바구니 테두리를 기준으로 잰다. 바구니를 벗어난 적재물은 바로 떨어져 0.5초 안에 손실되므로
		// 러너 기준 손실 거리(200cm)로는 경고가 거의 뜨지 않는다. 테두리를 향해 밀려나는 동안 경고가 켜져야 원인이 보인다.
		float RimRatio = 0.0f;
		if (bHasFootprint && CargoActor->GetCargoMeshComponent())
		{
			const FVector LocalOffset = GetCargoOffsetInSupport(*CargoActor->GetCargoMeshComponent(), SupportBounds);
			if (LocalOffset.Z < SupportBounds.BoxExtent.Z + SettledHeightAboveRim)
			{
				// 테두리 반지름 밖이거나 바닥 아래면 이미 떨어지는 중이라 1로 본다. 아직 위에서 내려오는 적재물은 세지 않는다.
				const FVector2D RimOffset(LocalOffset);
				const bool bLeftBowl = RimOffset.SizeSquared() > FMath::Square(RimRadius) || LocalOffset.Z < -SupportBounds.BoxExtent.Z;
				RimRatio = bLeftBowl ? 1.0f : RimOffset.Size() / RimRadius;
				if (RimRatio > MaxRimRatio)
				{
					MaxRimRatio = RimRatio;
					MaxRimOffset = RimOffset;
				}
			}
		}
		CargoInstances[CargoIndex].DangerRatio = FMath::Clamp(RimRatio, 0.0f, 1.0f);
	}

	if (bHasFootprint)
	{
		CargoDangerRatio = FMath::Clamp(MaxRimRatio, 0.0f, 1.0f);
		CargoDangerDirection = MaxRimOffset.GetSafeNormal();
	}
	else
	{
		// 바구니가 없으면 예전처럼 러너 기준 수평 이탈만 본다. 수직 낙하는 경고할 여지가 없다.
		CargoDangerRatio = LossDistanceSquared > KINDA_SMALL_NUMBER
			? FMath::Clamp(FMath::Sqrt(MaxHorizontalDistanceSquared / LossDistanceSquared), 0.0f, 1.0f)
			: 0.0f;
		CargoDangerDirection = FVector2D::ZeroVector;
	}

	return bAnyCargoLost;
}

void UPGCargoStackComponent::ApplyExternalLateralImpulse(float LateralVelocity)
{
	if (FMath::IsNearlyZero(LateralVelocity))
	{
		return;
	}

	const FVector Impulse = GetRightVector() * LateralVelocity;
	for (const FPGRuntimeCargoInstance& CargoInstance : CargoInstances)
	{
		UStaticMeshComponent* CargoMeshComponent = CargoInstance.CargoActor ? CargoInstance.CargoActor->GetCargoMeshComponent() : nullptr;
		if (CargoMeshComponent)
		{
			// 질량이 적재물마다 달라도 같은 속도만큼 밀리도록 속도 변화로 더한다.
			CargoMeshComponent->AddImpulse(Impulse, NAME_None, true);
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
