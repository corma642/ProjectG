// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGCargoStackComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Go Deliver!의 머리 위 적재물 개수, 슬롯 위치, 시각 흔들림을 관리하는 컴포넌트
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// Project Settings의 적재물 설정을 적용하고 시작 적재물을 생성한다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 러너 입력과 라인 점프 상태를 적재물 시각 반응에 반영한다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane);

	// 적재물을 1개 추가하고 최대 개수를 넘으면 실패를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargo();

	// 적재물을 1개 제거하고 더 제거할 수 없으면 실패를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool RemoveCargo();

	// 현재 머리 위에 유지 중인 적재물 개수
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CurrentCargoCount; }

	// 현재 적용 중인 적재물 흔들림 롤 각도
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCurrentSwayRoll() const { return CurrentSwayRoll; }

private:
	// 최대 적재 개수만큼 표시용 메시 슬롯 풀을 준비한다.
	void EnsureCargoMeshPool();

	// 현재 적재 개수에 맞게 메시 슬롯 표시 상태를 갱신한다.
	void RefreshCargoSlotVisibility();

	// 각 적재 슬롯의 위치와 회전을 현재 흔들림/점프 상태에 맞게 갱신한다.
	void UpdateSlotTransforms();

	// 메시가 없거나 디버그 표시가 켜진 경우 적재 슬롯 위치를 시각화한다.
	void DrawCargoDebug() const;

	// 지정한 인덱스의 적재물 상대 위치를 계산한다.
	FVector GetSlotRelativeLocation(int32 SlotIndex) const;

	// BeginPlay에서 캐시한 적재물 설정
	FPGCargoSettings CachedCargoSettings;

	// 적재물 표시용 Static Mesh. 비워두면 디버그 구체만 표시할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> CargoMesh;

	// 런타임에 생성한 적재물 메시 컴포넌트 목록
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CargoMeshComponents;

	// 현재 머리 위에 유지 중인 적재물 개수
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	int32 CurrentCargoCount = 0;

	// 현재 적용 중인 흔들림 롤 각도
	float CurrentSwayRoll = 0.0f;

	// 현재 라인 점프에 의해 추가된 시각 상승 높이
	float CurrentJumpLift = 0.0f;
};
