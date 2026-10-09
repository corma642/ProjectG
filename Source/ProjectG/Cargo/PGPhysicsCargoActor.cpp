// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGPhysicsCargoActor.h"

#include "Cargo/PGCargoTypeDefinition.h"
#include "Components/StaticMeshComponent.h"

APGPhysicsCargoActor::APGPhysicsCargoActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CargoMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMeshComponent);

	// 적재물은 받침/월드와만 물리 충돌하고, 캐릭터 캡슐과는 충돌하지 않는다.
	CargoMeshComponent->SetMobility(EComponentMobility::Movable);
	CargoMeshComponent->SetSimulatePhysics(true);
	CargoMeshComponent->SetEnableGravity(true);
	CargoMeshComponent->SetGenerateOverlapEvents(false);
	CargoMeshComponent->SetNotifyRigidBodyCollision(true);
	CargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CargoMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// 머리 위에서 튕겨 나간 적재물이 카메라 붐을 가로지르면 붐이 장애물로 보고 카메라를 캐릭터 안으로 당긴다. 적재물은 카메라를 가리지 않는 것으로 둔다.
	CargoMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CargoMeshComponent->OnComponentHit.AddDynamic(this, &APGPhysicsCargoActor::HandleCargoMeshHit);
}

void APGPhysicsCargoActor::InitializeCargo(UPGCargoTypeDefinition* InCargoType, FName CollisionProfileName)
{
	CargoType = InCargoType;
	if (!CargoMeshComponent || !CargoType)
	{
		return;
	}

	CargoMeshComponent->SetStaticMesh(CargoType->CargoMesh);
	CargoMeshComponent->SetRelativeRotation(CargoType->MeshRotation);
	CargoMeshComponent->SetRelativeScale3D(CargoType->MeshScale);

	if (!CollisionProfileName.IsNone())
	{
		CargoMeshComponent->SetCollisionProfileName(CollisionProfileName);
	}

	// 프로파일을 다시 적용하면 채널 응답이 초기화되므로 캐릭터와 카메라 붐 무시를 다시 건다.
	CargoMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CargoMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CargoMeshComponent->SetNotifyRigidBodyCollision(true);
	CargoMeshComponent->SetLinearDamping(FMath::Max(0.0f, CargoType->LinearDamping));
	CargoMeshComponent->SetAngularDamping(FMath::Max(0.0f, CargoType->AngularDamping));

	if (CargoType->MassInKg > 0.0f)
	{
		CargoMeshComponent->SetMassOverrideInKg(NAME_None, CargoType->MassInKg, true);
	}

	CargoMeshComponent->SetSimulatePhysics(true);
	CargoMeshComponent->WakeAllRigidBodies();
}

void APGPhysicsCargoActor::HandleCargoMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	LimitImpactBounceVelocity();
	LimitAngularVelocity();
}

void APGPhysicsCargoActor::LimitImpactBounceVelocity() const
{
	if (!CargoMeshComponent || !CargoType)
	{
		return;
	}

	FVector LinearVelocity = CargoMeshComponent->GetPhysicsLinearVelocity();
	if (LinearVelocity.Z <= 0.0f)
	{
		return;
	}

	// 적재물이 받침에 닿은 직후 튀어오르는 성분만 줄여, 낙하/좌우 물리감은 유지한다.
	const float BounceScale = FMath::Clamp(CargoType->ImpactBounceVelocityScale, 0.0f, 1.0f);
	const float MaxUpwardSpeed = FMath::Max(0.0f, CargoType->MaxUpwardBounceSpeed);
	LinearVelocity.Z = FMath::Min(LinearVelocity.Z * BounceScale, MaxUpwardSpeed);
	CargoMeshComponent->SetPhysicsLinearVelocity(LinearVelocity);
}

void APGPhysicsCargoActor::LimitAngularVelocity() const
{
	if (!CargoMeshComponent || !CargoType)
	{
		return;
	}

	const float MaxAngularSpeedRadians = FMath::DegreesToRadians(FMath::Max(0.0f, CargoType->MaxAngularSpeedDegrees));
	if (MaxAngularSpeedRadians <= KINDA_SMALL_NUMBER)
	{
		CargoMeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		return;
	}

	const FVector AngularVelocity = CargoMeshComponent->GetPhysicsAngularVelocityInRadians();
	const float AngularSpeed = AngularVelocity.Size();
	if (AngularSpeed <= MaxAngularSpeedRadians)
	{
		return;
	}

	// 작은 적재물 메시가 충돌 순간 과도하게 회전해 튕겨나가는 현상을 막는다.
	CargoMeshComponent->SetPhysicsAngularVelocityInRadians(AngularVelocity.GetSafeNormal() * MaxAngularSpeedRadians);
}
