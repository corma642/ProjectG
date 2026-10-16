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

/** 적재물이 손실 범위를 벗어나 제거됐을 때 제거된 리스트 인덱스와 함께 발생합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPGCargoLostSignature, int32);
/** 배달 완료 시 적재물 instance ID, 배달 점수, 영수증과 월드 위치를 HUD/진행도에 전달합니다. */
DECLARE_MULTICAST_DELEGATE_FourParams(FPGCargoDeliveredSignature, int32, int32, int32, FVector);
/** 활성 적재물 목록 또는 남은 배달 시간이 바뀌어 HUD 카드 동기화가 필요할 때 발생합니다. */
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

	// 이 적재물의 손실 임박 비율(0~1, 바구니 테두리 반지름 대비 중심 이탈). 카드 경고 표시가 읽습니다.
	UPROPERTY(BlueprintReadOnly, Category = "Cargo")
	float DangerRatio = 0.0f;
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
	// 직전 손실 판정의 바구니 테두리 대비 중심 이탈(0~1). HUD 카드 경고가 읽습니다.
	float DangerRatio = 0.0f;
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
	/** 러너가 명시적으로 갱신하는 SceneComponent와 scripted cargo 표시 상태를 구성합니다. */
	UPGCargoStackComponent();

	/** 설정 적용 전에 런타임 표시 컴포넌트와 소유 러너 참조를 준비합니다. */
	virtual void BeginPlay() override;

	/** 생성한 물리 적재물과 Hold 표시 컴포넌트를 월드 종료 전에 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 배달 적재물이 플레이어 기준 손실 범위를 벗어나면 제거된 리스트 인덱스와 함께 발생합니다. */
	FPGCargoLostSignature OnCargoLost;

	/** 배달 완료 시 instance ID, 점수, 영수증과 월드 위치를 보상 처리 계층에 전달합니다. */
	FPGCargoDeliveredSignature OnCargoDelivered;

	/** 활성 적재물 목록 또는 시간 표시가 바뀌어 HUD 카드 갱신이 필요할 때 발생합니다. */
	FPGCargoListChangedSignature OnCargoListChanged;

	/** Project Settings의 적재물 물리·배달 튜닝값을 적용하고 새 런 기준 상태로 초기화합니다. */
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	/** 현재 맵에서 사용할 고정 월드 Z 손실선(cm)을 적용합니다. */
	void SetCargoLossWorldZ(float InCargoLossWorldZ);

	/**
	 * 적재물을 실제로 받치는 컴포넌트(바구니)를 지정합니다. 이후 바구니에 담긴 적재물의 공중 속도 보정은 이 컴포넌트의 움직임을 기준으로 합니다.
	 * null이면 모든 적재물이 CargoStack 자신의 움직임을 기준으로 삼습니다.
	 */
	void SetSupportComponent(const USceneComponent* InSupportComponent);

	/** -1~1 중심 입력과 공중 상태로 Hold, 물리 보정, 배달 완료와 손실 판정을 한 프레임 갱신합니다. */
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, bool bIsAirborne);

	/** 지정한 적재물 타입으로 CargoStack 위치의 Hold 연출을 시작하고, 완료 후 물리 적재물로 전환합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	/** 현재 실제 물리 Actor로 존재하는 배달 적재물 개수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoInstances.Num(); }

	/** 물리 적재물과 Hold 대기 중인 적재물을 모두 포함한 예약 개수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetReservedCargoCount() const { return CargoInstances.Num() + (bHasPendingCargoDrop ? 1 : 0); }

	/** 현재 런에서 배달 완료 처리된 적재물 개수를 반환합니다. 런 reset 시 0으로 돌아갑니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetDeliveredCargoCount() const { return DeliveredCargoCount; }

	/** 물리 전환 전 Hold 메시로 대기 중인 적재물이 있는지 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	bool HasPendingCargoDrop() const { return bHasPendingCargoDrop; }

	/** 바구니에 담긴 적재물 중 가장 바깥쪽 것의 중심 이탈을 바구니 테두리 반지름 대비 0~1로 반환합니다. 테두리를 넘어 떨어지는 중이면 1, 적재물이 없으면 0입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCargoDangerRatio() const { return CargoDangerRatio; }

	/** GetCargoDangerRatio를 만든 적재물이 바구니 중심에서 밀려난 방향(바구니 로컬 XY, 단위 벡터)입니다. 위험이 없으면 0입니다. */
	FVector2D GetCargoDangerDirection() const { return CargoDangerDirection; }

	/** 위험 Actor 피격처럼 외부 충격이 났을 때 모든 활성 적재물에 러너 우측 축 속도(cm/s)를 더합니다. */
	void ApplyExternalLateralImpulse(float LateralVelocity);

	/** HUD용 활성 적재물 스냅샷을 OutInfos에 채웁니다. 남은 시간은 초, 진행률은 0~1 범위이며 기존 내용은 비웁니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	void GetActiveCargoInfos(TArray<FPGActiveCargoInfo>& OutInfos) const;

private:
	bool IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const;
	void EnsureScriptedCargoMeshComponent();
	void DestroyScriptedCargoMeshComponent();
	bool StartCargoDropHold(UPGCargoTypeDefinition* CargoType);
	// PendingPhysicsSeconds: 생성한 뒤 같은 프레임 물리가 진행할 시간(초). 러너 Tick에서 부르면 프레임 시간, 물리 뒤에 부르면 0입니다.
	void FinishCargoDropHold(float PendingPhysicsSeconds);
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);
	FTransform MakeCargoWorldTransform() const;
	void ApplyScriptedCargoDisplayTransform(const UPGCargoTypeDefinition* CargoType);
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);
	void UpdateCargoDeliveries(float DeltaSeconds);
	void UpdateAnchorMotion(float DeltaSeconds, bool bIsAirborne);
	void ApplyAirborneCargoAssist();
	// 바구니 루트의 로컬 범위와 테두리 반지름을 구합니다. 바구니가 없거나 크기가 없으면 false입니다.
	bool GetSupportFootprint(FBoxSphereBounds& OutBounds, float& OutRimRadius) const;
	// 적재물 위치(지난 물리 스텝)를 바구니 기준 로컬 오프셋으로 바꿉니다.
	FVector GetCargoOffsetInSupport(const UStaticMeshComponent& CargoMeshComponent, const FBoxSphereBounds& SupportBounds) const;
	FVector GetLossOriginLocation() const;
	bool CheckCargoLoss(float DeltaSeconds);
	void LoseCargoAt(int32 CargoIndex);
	void CompleteCargoAt(int32 CargoIndex);
	void CleanupInvalidCargoActors();

	FPGCargoSettings CachedCargoSettings;

	// 적재물을 받치는 바구니 컴포넌트입니다. 머리 소켓에 붙어 애니메이션만큼 캡슐과 다르게 움직이므로 속도 기준을 따로 둡니다.
	TWeakObjectPtr<const USceneComponent> SupportComponent;

	// 바구니의 이번 프레임 자세와, 적재물의 현재 위치가 계산된 지난 물리 스텝 시점의 자세입니다.
	FTransform LatestSupportTransform = FTransform::Identity;
	FTransform SupportTransformAtLastStep = FTransform::Identity;

	// 바구니가 이번 프레임에 움직인 속도(cm/s)입니다. 바구니가 없으면 AnchorVelocity와 같습니다.
	FVector SupportVelocity = FVector::ZeroVector;
	bool bHasLatestSupportTransform = false;

	// Hold 중인 적재물을 보여주는 비물리 프리뷰 메시입니다. 실제 물리 Actor는 Hold 종료 후 별도로 생성됩니다.
	UPROPERTY(Transient)
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
	float CargoLossWorldZ = 0.0f;
	// 직전 손실 판정에서 계산한 손실 임박 비율(0~1, 바구니 테두리 반지름 대비 최대 중심 이탈). HUD 손실 임박 경고가 읽습니다.
	float CargoDangerRatio = 0.0f;
	// CargoDangerRatio를 만든 적재물의 바구니 로컬 XY 이탈 방향(단위 벡터). 바구니 테두리 경고 링이 켜질 쪽입니다.
	FVector2D CargoDangerDirection = FVector2D::ZeroVector;
	bool bWasAirborneLastFrame = false;
	bool bHasPreviousAnchorLocation = false;
	bool bHasCargoLossWorldZ = false;
	bool bWarnedMissingCargoLossWorldZ = false;
	int32 NextCargoInstanceId = 1;
	int32 DeliveredCargoCount = 0;
};
