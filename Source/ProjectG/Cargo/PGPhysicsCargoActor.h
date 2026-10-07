// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGPhysicsCargoActor.generated.h"

class UPGCargoTypeDefinition;
class UPrimitiveComponent;
class UStaticMeshComponent;

/**
 * 바구니에 담기는 실제 물리 적재물 Actor입니다.
 * 캐릭터나 바구니에 부착되지 않고, Static Mesh 물리 시뮬레이션으로만 움직입니다.
 */
UCLASS()
class PROJECTG_API APGPhysicsCargoActor : public AActor
{
	GENERATED_BODY()

public:
	/** 물리 시뮬레이션용 StaticMeshComponent와 충돌 피드백 delegate를 구성합니다. */
	APGPhysicsCargoActor();

	/** 지정한 적재물 DataAsset의 메시·질량·감쇠와 collision profile을 적용합니다. null이면 초기화하지 않습니다. */
	void InitializeCargo(UPGCargoTypeDefinition* InCargoType, FName CollisionProfileName);

	/** 물리 시뮬레이션과 충돌을 담당하는 StaticMeshComponent를 반환합니다. */
	UStaticMeshComponent* GetCargoMeshComponent() const { return CargoMeshComponent; }

private:
	// 물리 충돌 직후 과도한 위쪽 반발 속도와 회전 속도를 제한한다.
	UFUNCTION()
	void HandleCargoMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 설정값을 기준으로 충돌 후 튀어오르는 속도를 낮춘다.
	void LimitImpactBounceVelocity() const;

	// 설정값을 기준으로 충돌 후 과도한 회전 속도를 낮춘다.
	void LimitAngularVelocity() const;

	// 물리 시뮬레이션과 충돌을 담당하는 적재물 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CargoMeshComponent;

	// 런타임에 적용된 적재물 타입
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> CargoType;
};
