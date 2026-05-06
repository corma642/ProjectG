// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGCargoStackComponent.generated.h"

class APGPhysicsCargoActor;
class UPGCargoTypeDefinition;
class UStaticMeshComponent;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FPGCargoLostSignature, int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPGCargoDeliveredSignature, int32, int32);
DECLARE_MULTICAST_DELEGATE(FPGCargoListChangedSignature);
DECLARE_MULTICAST_DELEGATE(FPGCargoSignalSignature);

/**
 * HUD가 적재물 썸네일과 배달 게이지를 표시할 때 사용할 공개 상태 정보입니다.
 */
USTRUCT(BlueprintType)
struct FPGActiveCargoInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UPGCargoTypeDefinition> CargoType;

	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UTexture2D> CargoThumbnail;

	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	float DeliveryProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	float RemainingDeliveryTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	float DeliveryDuration = 0.0f;
};

/**
 * 물리 Actor로 전환된 활성 적재물의 배달 상태입니다.
 */
USTRUCT()
struct FPGRuntimeCargoInstance
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<APGPhysicsCargoActor> CargoActor;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> CargoType;

	int32 InstanceId = INDEX_NONE;
	float DeliveryElapsedTime = 0.0f;
};

/**
 * 플레이어에 붙어 적재물 생성/대기/속도 보정 기준을 제공하는 컴포넌트입니다.
 * 바구니 메시와 콜리전은 BP 책임이며, 이 컴포넌트는 물리 적재물 생성, 배달 완료, 손실 판정만 담당합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 적재물이 플레이어 기준 손실 범위를 벗어나면 발생합니다.
	FPGCargoLostSignature OnCargoLost;

	// 적재물이 배달 완료 시간만큼 유지되어 성공 처리될 때 발생합니다. InstanceId와 추가 점수를 전달합니다.
	FPGCargoDeliveredSignature OnCargoDelivered;

	// 활성 적재물 목록이 바뀌어 HUD 갱신이 필요할 때 발생합니다.
	FPGCargoListChangedSignature OnCargoListChanged;

	// 적재물이 비물리 대기 상태로 표시될 때 발생합니다.
	FPGCargoSignalSignature OnCargoDropHoldStarted;

	// 적재물 대기가 끝나고 실제 물리 Actor로 전환될 때 발생합니다.
	FPGCargoSignalSignature OnCargoDropHoldFinished;

	// Project Settings의 적재 설정을 적용하고 새 런 시작 전 상태로 초기화합니다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 중심 입력, 적재물 대기 타이머, 물리 보정, 배달 완료, 손실 판정을 갱신합니다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, bool bIsChangingLane);

	// 기본 적재물 타입으로 적재물 대기/드롭 흐름을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargo();

	// 지정한 적재물 타입으로 적재물 대기/드롭 흐름을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	// 실제 물리 Actor로 존재하는 적재물 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoInstances.Num(); }

	// 현재 물리 적재물과 추가 예정 적재물을 포함한 예약 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetReservedCargoCount() const { return CargoInstances.Num() + (bHasPendingCargoDrop ? 1 : 0); }

	// 현재 배달 완료된 적재물 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetDeliveredCargoCount() const { return DeliveredCargoCount; }

	// 현재 Hold 상태의 적재물이 있는지 여부입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	bool HasPendingCargoDrop() const { return bHasPendingCargoDrop; }

	// 추가 적재물이 대기 위치에 머무르는 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCargoDropHoldRemainingTime() const { return CargoDropHoldRemainingTime; }

	// HUD가 활성 적재물 썸네일과 배달 게이지를 표시할 때 사용하는 상태 목록입니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	TArray<FPGActiveCargoInfo> GetActiveCargoInfos() const;

private:
	bool IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const;
	bool StartCargoDropHold(UPGCargoTypeDefinition* CargoType);
	void FinishCargoDropHold();
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);
	UPGCargoTypeDefinition* ResolveDefaultCargoType() const;
	FTransform MakeCargoWorldTransform() const;
	void ApplyScriptedCargoDisplayTransform(const UPGCargoTypeDefinition* CargoType);
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);
	void UpdateCargoDeliveries(float DeltaSeconds);
	void UpdateAnchorMotion(float DeltaSeconds, bool bIsChangingLane);
	void ApplyLaneJumpCargoAssist();
	FVector GetLossOriginLocation() const;
	void CheckCargoLoss();
	void LoseCargoAt(int32 CargoIndex);
	void CompleteCargoAt(int32 CargoIndex);
	void CleanupInvalidCargoActors();
	void DrawCargoDebug() const;

	FPGCargoSettings CachedCargoSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ScriptedCargoMeshComponent;

	UPROPERTY(Transient)
	TArray<FPGRuntimeCargoInstance> CargoInstances;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> PendingCargoType;

	bool bHasPendingCargoDrop = false;
	float CargoDropHoldRemainingTime = 0.0f;
	float CargoShakeElapsedTime = 0.0f;
	FVector PreviousAnchorLocation = FVector::ZeroVector;
	FVector AnchorVelocity = FVector::ZeroVector;
	float LaneJumpCargoAssistRemainingTime = 0.0f;
	bool bWasChangingLaneLastFrame = false;
	bool bHasPreviousAnchorLocation = false;
	int32 NextCargoInstanceId = 1;
	int32 DeliveredCargoCount = 0;
};
