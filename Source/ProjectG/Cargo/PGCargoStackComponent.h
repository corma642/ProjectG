// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGCargoStackComponent.generated.h"

class APGPhysicsCargoActor;
class UPGCargoTypeDefinition;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FPGCargoLostSignature, int32);
DECLARE_MULTICAST_DELEGATE(FPGCargoSignalSignature);

/**
 * 러너 기준 적재물 생성/대기/속도 보정 앵커입니다.
 * 바구니 메시와 충돌 구성은 Blueprint 책임이고, 이 컴포넌트는 적재물 Actor 생성과 손실 판정만 담당합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 적재물이 플레이어 기준 손실 범위를 벗어났을 때 발생합니다.
	FPGCargoLostSignature OnCargoLost;

	// 추가 적재물이 비물리 대기 상태로 표시될 때 발생합니다.
	FPGCargoSignalSignature OnCargoDropHoldStarted;

	// 추가 적재물 대기가 끝나고 실제 물리 Actor로 전환될 때 발생합니다.
	FPGCargoSignalSignature OnCargoDropHoldFinished;

	// Project Settings의 적재 설정을 적용하고 런 시작 전 상태로 초기화합니다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 중심 입력, 추가 적재 대기 타이머, 적재물 물리 보정 및 손실 판정을 갱신합니다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, bool bIsChangingLane);

	// 기본 적재물 타입으로 추가 적재물 대기 흐름을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargo();

	// 지정한 적재물 타입으로 추가 적재물 대기 흐름을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	// 실제 물리 Actor로 존재하는 적재물 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoActors.Num(); }

	// 현재 물리 적재물과 추가 예정 적재물을 포함한 예약 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetReservedCargoCount() const { return CargoActors.Num() + (bHasPendingCargoDrop ? 1 : 0); }

	// Project Settings 기준 최대 적재 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetMaxCargoCount() const { return CachedCargoSettings.MaxCargoCount; }

	// 추가 적재물이 대기 위치에 머무르는 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCargoDropHoldRemainingTime() const { return CargoDropHoldRemainingTime; }

	// 실제 러닝 시작 시점에 첫 적재물을 CargoStack 위치 기준으로 투입합니다.
	void SpawnInitialCargo();

private:
	bool IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const;
	bool StartCargoDropHold(UPGCargoTypeDefinition* CargoType);
	void FinishCargoDropHold();
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);
	UPGCargoTypeDefinition* ResolveDefaultCargoType() const;
	FTransform MakeCargoWorldTransform(int32 CargoIndex = 0) const;
	void ApplyScriptedCargoDisplayTransform(const UPGCargoTypeDefinition* CargoType);
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);
	void UpdateAnchorMotion(float DeltaSeconds, bool bIsChangingLane);
	void ApplyLaneJumpCargoAssist();
	FVector GetLossOriginLocation() const;
	void CheckCargoLoss();
	void LoseCargoAt(int32 CargoIndex);
	void CleanupInvalidCargoActors();
	void DrawCargoDebug() const;

	FPGCargoSettings CachedCargoSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ScriptedCargoMeshComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APGPhysicsCargoActor>> CargoActors;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> PendingCargoType;

	bool bHasPendingCargoDrop = false;
	float InitialCargoSettleRemainingTime = 0.0f;
	float CargoDropHoldRemainingTime = 0.0f;
	float CargoShakeElapsedTime = 0.0f;
	FVector PreviousAnchorLocation = FVector::ZeroVector;
	FVector AnchorVelocity = FVector::ZeroVector;
	float LaneJumpCargoAssistRemainingTime = 0.0f;
	bool bWasChangingLaneLastFrame = false;
	bool bHasPreviousAnchorLocation = false;
};
