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
DECLARE_MULTICAST_DELEGATE_FourParams(FPGCargoDeliveredSignature, int32, int32, int32, FVector);
DECLARE_MULTICAST_DELEGATE(FPGCargoListChangedSignature);

/**
 * HUD가 활성 배달 적재물 카드와 배달 진행도를 그릴 때 사용하는 읽기 전용 상태입니다.
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
 * Hold 연출을 끝내고 물리 Actor로 전환된 배달 적재물의 런타임 상태입니다.
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
 * 러너에 부착되어 배달 적재물의 생성, Hold 표시, 물리 전환, 배달 완료, 손실 판정을 관리합니다.
 * 바구니 메시와 충돌은 BP/레벨 책임으로 두고, 이 컴포넌트는 CargoStack 위치를 적재물 기준점으로만 사용합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 배달 적재물이 플레이어 기준 손실 범위를 벗어나면 발생합니다.
	FPGCargoLostSignature OnCargoLost;

	// 배달 적재물이 완료 시간까지 유지되어 점수/영수증 보상 처리 직전에 발생합니다.
	FPGCargoDeliveredSignature OnCargoDelivered;

	// 활성 적재물 목록이 바뀌어 HUD 카드 갱신이 필요할 때 발생합니다.
	FPGCargoListChangedSignature OnCargoListChanged;

	// Project Settings의 적재물 물리/배달 튜닝값을 적용하고 새 런 기준 상태로 초기화합니다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 중심 입력, Hold 타이머, 물리 보정, 배달 완료, 손실 판정을 한 프레임 갱신합니다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, bool bIsAirborne);

	// 지정한 적재물 타입으로 CargoStack 위치의 Hold 연출을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	// 실제 물리 Actor로 존재하는 배달 적재물 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoInstances.Num(); }

	// 물리 적재물과 Hold 대기 중인 적재물을 모두 포함한 예약 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetReservedCargoCount() const { return CargoInstances.Num() + (bHasPendingCargoDrop ? 1 : 0); }

	// 현재 런에서 배달 완료 처리된 적재물 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetDeliveredCargoCount() const { return DeliveredCargoCount; }

	// 물리 전환 전 Hold 메시로 대기 중인 적재물이 있는지 여부입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	bool HasPendingCargoDrop() const { return bHasPendingCargoDrop; }

	// HUD가 썸네일, 남은 시간, 진행 바를 표시할 때 사용하는 활성 적재물 목록입니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	TArray<FPGActiveCargoInfo> GetActiveCargoInfos() const;

private:
	bool IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const;
	bool StartCargoDropHold(UPGCargoTypeDefinition* CargoType);
	void FinishCargoDropHold();
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);
	FTransform MakeCargoWorldTransform() const;
	void ApplyScriptedCargoDisplayTransform(const UPGCargoTypeDefinition* CargoType);
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);
	void UpdateCargoDeliveries(float DeltaSeconds);
	void UpdateAnchorMotion(float DeltaSeconds, bool bIsAirborne);
	void ApplyAirborneCargoAssist();
	FVector GetLossOriginLocation() const;
	void CheckCargoLoss();
	void LoseCargoAt(int32 CargoIndex);
	void CompleteCargoAt(int32 CargoIndex);
	void CleanupInvalidCargoActors();

	FPGCargoSettings CachedCargoSettings;

	// Hold 중인 적재물을 보여주는 비물리 프리뷰 메시입니다. 실제 물리 Actor는 Hold 종료 후 별도로 생성됩니다.
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
	float AirborneCargoAssistRemainingTime = 0.0f;
	bool bWasAirborneLastFrame = false;
	bool bHasPreviousAnchorLocation = false;
	int32 NextCargoInstanceId = 1;
	int32 DeliveredCargoCount = 0;
};
