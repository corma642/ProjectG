// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGCargoStackComponent.generated.h"

class UPGCargoTypeDefinition;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 적재물이 현재 얼마나 위험한 상태인지 나타내는 안정도 단계
 */
UENUM(BlueprintType)
enum class EPGCargoStabilityState : uint8
{
	Stable,
	Warning,
	Critical
};

/**
 * 머리 위 슬롯 하나에 올라간 적재물의 타입과 현재 안정도 상태
 */
USTRUCT(BlueprintType)
struct FPGCargoInstance
{
	GENERATED_BODY()

	// 이 슬롯에 올라간 적재물 타입. 비어 있으면 기본 핵심 계란처럼 취급한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UPGCargoTypeDefinition> CargoType;

	// 이 슬롯 적재물이 손실됐을 때 즉시 게임오버로 이어지는 핵심 적재물인지 여부
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo")
	bool bIsCoreCargoInstance = true;

	// 손실 임계값에 가까워지는 현재 불안정도 값
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo")
	float StabilityValue = 0.0f;

	// 현재 안정도 단계
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo")
	EPGCargoStabilityState StabilityState = EPGCargoStabilityState::Stable;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGCargoStateChangedSignature, int32, SlotIndex, EPGCargoStabilityState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGCargoLostSignature, int32, SlotIndex, bool, bWasCoreCargo);

/**
 * Go Deliver!의 머리 위 적재물 타입, 슬롯 위치, 안정도, 시각 흔들림, 손실 이벤트를 관리하는 컴포넌트
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 적재물 안정도 단계가 바뀔 때 호출되는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Cargo")
	FPGCargoStateChangedSignature OnCargoStateChanged;

	// 적재물이 손실될 때 호출되는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Cargo")
	FPGCargoLostSignature OnCargoLost;

	// Project Settings의 적재물 설정을 적용하고 시작 적재물을 생성한다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 러너 입력과 라인 점프 상태를 적재물 안정도와 시각 반응에 반영한다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane);

	// 기본 적재물 타입으로 적재물을 1개 추가하고 최대 개수를 넘으면 실패를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargo();

	// 지정한 적재물 타입으로 1개를 추가한다. 타입이 비어 있으면 추가하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	// 설정된 보너스 적재물 타입 또는 Mesh 없는 비핵심 fallback 적재물을 1개 추가한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddBonusCargo();

	// 가장 위의 적재물을 1개 제거하고 더 제거할 수 없으면 실패를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool RemoveCargo();

	// 현재 머리 위에 유지 중인 적재물 개수
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoInstances.Num(); }

	// 현재 적용 중인 적재물 흔들림 롤 각도
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCurrentSwayRoll() const { return CurrentSwayRoll; }

	// 지정 슬롯의 안정도 값을 반환한다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCargoStabilityValue(int32 SlotIndex) const;

	// 지정 슬롯의 안정도 단계를 반환한다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	EPGCargoStabilityState GetCargoStabilityState(int32 SlotIndex) const;

private:
	// 기본 타입을 사용해 새 적재물 슬롯 인스턴스를 만든다.
	FPGCargoInstance MakeDefaultCargoInstance() const;

	// 타입과 fallback 핵심 여부를 기준으로 슬롯 인스턴스를 생성한다.
	FPGCargoInstance MakeCargoInstance(UPGCargoTypeDefinition* CargoType, bool bFallbackCoreCargo) const;

	// 최대 적재 개수만큼 표시용 메시 슬롯 풀을 준비한다.
	void EnsureCargoMeshPool();

	// 현재 적재 슬롯에 맞게 메시 슬롯 표시 상태를 갱신한다.
	void RefreshCargoSlotVisibility();

	// 각 적재 슬롯의 위치와 회전을 현재 흔들림/점프 상태에 맞게 갱신한다.
	void UpdateSlotTransforms();

	// 모든 적재물의 안정도 손상/회복과 상태 변화를 갱신한다.
	void UpdateCargoStability(float DeltaSeconds, float BalanceInput, bool bIsChangingLane);

	// 지정 슬롯의 안정도 상태를 현재 안정도 값에 맞게 계산한다.
	EPGCargoStabilityState CalculateStabilityState(float StabilityValue) const;

	// 지정 슬롯의 적재물 손상/흔들림 계수를 반환한다.
	float GetCargoDamageMultiplier(const FPGCargoInstance& CargoInstance) const;
	float GetCargoSwayMultiplier(const FPGCargoInstance& CargoInstance) const;

	// 지정 슬롯이 핵심 적재물인지 반환한다.
	bool IsCoreCargo(const FPGCargoInstance& CargoInstance) const;

	// 지정 슬롯의 표시용 메시를 반환한다.
	UStaticMesh* GetCargoMesh(const FPGCargoInstance& CargoInstance) const;

	// 지정 슬롯의 적재물을 손실 처리하고 이벤트를 발생시킨다.
	void LoseCargoAt(int32 SlotIndex);

	// 메시가 없거나 디버그 표시가 켜진 경우 적재 슬롯 위치를 시각화한다.
	void DrawCargoDebug() const;

	// 지정한 인덱스의 적재물 상대 위치를 계산한다.
	FVector GetSlotRelativeLocation(int32 SlotIndex) const;

	// BeginPlay에서 캐시한 적재물 설정
	FPGCargoSettings CachedCargoSettings;

	// 현재 머리 위에 올라간 적재물 슬롯 목록
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TArray<FPGCargoInstance> CargoInstances;

	// 런타임에 생성한 적재물 메시 컴포넌트 풀
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CargoMeshComponents;

	// 현재 적용 중인 흔들림 롤 각도
	float CurrentSwayRoll = 0.0f;

	// 현재 라인 점프에 의해 추가된 시각 상승 높이
	float CurrentJumpLift = 0.0f;

	// 직전 프레임에 라인 이동 중이었는지 여부. 점프 손상을 한 번만 적용하기 위해 사용한다.
	bool bWasChangingLaneLastFrame = false;
};
