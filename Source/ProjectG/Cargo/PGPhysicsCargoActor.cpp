// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/PGPhysicsCargoActor.h"

#include "Cargo/PGCargoTypeDefinition.h"
#include "Components/StaticMeshComponent.h"

APGPhysicsCargoActor::APGPhysicsCargoActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CargoMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMeshComponent);

	// 계란은 바구니/월드와만 물리 충돌하고, 캐릭터 캡슐과는 충돌하지 않는다.
	CargoMeshComponent->SetMobility(EComponentMobility::Movable);
	CargoMeshComponent->SetSimulatePhysics(true);
	CargoMeshComponent->SetEnableGravity(true);
	CargoMeshComponent->SetGenerateOverlapEvents(false);
	CargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CargoMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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

	CargoMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CargoMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CargoMeshComponent->SetLinearDamping(FMath::Max(0.0f, CargoType->LinearDamping));
	CargoMeshComponent->SetAngularDamping(FMath::Max(0.0f, CargoType->AngularDamping));

	if (CargoType->MassInKg > 0.0f)
	{
		CargoMeshComponent->SetMassOverrideInKg(NAME_None, CargoType->MassInKg, true);
	}

	CargoMeshComponent->SetSimulatePhysics(true);
	CargoMeshComponent->WakeAllRigidBodies();
}
