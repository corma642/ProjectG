// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCargoTypeDefinition.generated.h"

class UStaticMesh;

/**
 * 물리 적재물 1종의 표시 메시와 물리 반응값을 정의하는 데이터 에셋입니다.
 * 현재 기획에서는 모든 적재물이 핵심 계란으로 취급되며, 하나라도 손실되면 런 실패로 이어집니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGCargoTypeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 에디터와 디버그 표시에서 읽기 쉬운 적재물 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	FText DisplayName;

	// 실제 월드에 물리 오브젝트로 스폰할 적재물 Static Mesh
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UStaticMesh> CargoMesh;

	// 메시 피벗이나 모델 크기가 맞지 않을 때 적용할 시각 스케일
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Visual")
	FVector MeshScale = FVector(1.0f);

	// 메시 피벗 방향이 월드 기준과 맞지 않을 때 적용할 초기 회전
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Visual")
	FRotator MeshRotation = FRotator::ZeroRotator;

	// 계란 물리 질량. 0보다 크면 Static Mesh 기본 질량 대신 이 값을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float MassInKg = 0.05f;

	// 계란의 선형 속도 감쇠. 값이 높을수록 바구니 안에서 덜 미끄러진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float LinearDamping = 0.15f;

	// 계란의 회전 속도 감쇠. 값이 높을수록 굴러다니는 속도가 빨리 줄어든다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float AngularDamping = 0.4f;

	// 충돌 직후 위쪽 반발 속도에 곱하는 비율. 낮을수록 덜 튀어오른다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ImpactBounceVelocityScale = 0.25f;

	// 충돌 직후 허용할 최대 위쪽 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float MaxUpwardBounceSpeed = 80.0f;

	// 충돌 후 과도한 회전 튐을 막기 위한 최대 각속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo | Physics", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3600.0"))
	float MaxAngularSpeedDegrees = 720.0f;
};
