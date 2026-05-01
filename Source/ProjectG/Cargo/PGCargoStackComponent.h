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
 * 플레이어에 부착된 물리 적재물 스폰/손실 판정 앵커입니다.
 * 바구니 메시와 충돌은 Blueprint 책임이며, 이 컴포넌트는 적재물 생성 위치와 손실 판정만 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 적재물이 CargoStack 기준 손실 범위를 벗어났을 때 발생하는 이벤트입니다.
	FPGCargoLostSignature OnCargoLost;

	// 시작 적재물 인트로 연출이 시작될 때 BP 카메라/시퀀스가 구독할 수 있는 이벤트입니다.
	FPGCargoSignalSignature OnInitialCargoIntroStarted;

	// 시작 적재물 인트로가 끝나고 실제 물리 적재물로 전환됐을 때 발생하는 이벤트입니다.
	FPGCargoSignalSignature OnInitialCargoIntroFinished;

	// 추가 적재물이 물리 전환 전 대기 위치에 표시될 때 발생하는 이벤트입니다.
	FPGCargoSignalSignature OnCargoDropHoldStarted;

	// 추가 적재물 대기가 끝나고 실제 물리 적재물로 전환됐을 때 발생하는 이벤트입니다.
	FPGCargoSignalSignature OnCargoDropHoldFinished;

	// Project Settings의 적재 설정을 적용하고 시작 적재물 흐름을 초기화합니다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 중심 입력, 추가 적재 대기 타이머, 적재물 물리 보정 및 손실 판정을 갱신합니다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane);

	// 기본 적재물 타입으로 새 적재물 추가 대기 흐름을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargo();

	// 지정한 적재물 타입으로 새 적재물 추가 대기 흐름을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	// 가장 마지막 적재물을 손실 처리합니다. 디버그 테스트용으로 사용합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool RemoveCargo();

	// 현재 살아 있는 물리 적재물 수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoActors.Num(); }

	// 현재 적재 중이거나 낙하 예약된 적재물까지 포함한 개수입니다. 자동 적재 이벤트 예산 판단에 사용합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetReservedCargoCount() const { return CargoActors.Num() + (bHasPendingCargoDrop ? 1 : 0) + (bIsInitialCargoIntroActive ? PendingInitialCargoCount : 0); }

	// Project Settings 기준 최대 적재 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetMaxCargoCount() const { return CachedCargoSettings.MaxCargoCount; }

	// 시작 적재물 인트로가 진행 중인지 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	bool IsInitialCargoIntroActive() const { return bIsInitialCargoIntroActive; }

	// 추가 적재물이 대기 위치에 머무르는 남은 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCargoDropHoldRemainingTime() const { return CargoDropHoldRemainingTime; }

private:
	// BP에서 앵커 위치를 지정하지 않은 경우 설정값 기반 기본 위치를 적용합니다.
	void ApplyFallbackAnchorLocation();

	// 지정한 적재물 타입이 스폰 가능한지 확인하고 실패 원인을 로그로 남깁니다.
	bool IsValidCargoTypeForSpawn(const UPGCargoTypeDefinition* CargoType, const TCHAR* Context) const;

	// 시작 적재물을 설정에 따라 즉시 스폰하거나 인트로 흐름으로 넘깁니다.
	void SpawnInitialCargo();

	// 시작 인트로 잠금을 시작하고 종료 시 첫 적재물을 실제 물리 Actor로 투입합니다.
	void StartInitialCargoIntro(UPGCargoTypeDefinition* CargoType, int32 InitialCount);

	// 시작 인트로 잠금 시간과 안정화 시간을 갱신합니다.
	void UpdateInitialCargoIntro(float DeltaSeconds);

	// 시작 인트로 잠금을 종료하고 첫 적재물을 생성합니다.
	void FinishInitialCargoIntro();

	// 적재물 추가 요청 시 플레이어를 따라가는 대기 상태로 전환합니다.
	bool StartCargoDropHold(UPGCargoTypeDefinition* CargoType);

	// 대기 시간이 끝난 뒤 앵커 근처의 투입 위치에서 실제 물리 적재물을 생성합니다.
	void FinishCargoDropHold();

	// 지정한 Transform에 물리 적재물 Actor를 생성합니다.
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);

	// DefaultCargoType을 동기 로드해 반환합니다.
	UPGCargoTypeDefinition* ResolveDefaultCargoType() const;

	// CargoStackComponent 기준 로컬 위치를 월드 Transform으로 변환합니다.
	FTransform MakeCargoWorldTransform(const FVector& LocalOffset, int32 CargoIndex = 0) const;

	// 살아 있는 적재물에 중심 흔들림 보조 힘을 적용합니다.
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);

	// 현재 앵커 속도와 라인 점프 보정 시간을 갱신합니다.
	void UpdateAnchorMotion(float DeltaSeconds, bool bIsChangingLane);

	// 라인 이동 중 또는 착지 직후 적재물의 앵커 대비 상대속도를 제한합니다.
	void ApplyLaneJumpCargoAssist();

	// 적재물이 플레이어 앵커 기준 손실 범위를 벗어났는지 확인합니다.
	void CheckCargoLoss();

	// 지정한 적재물을 제거하고 손실 이벤트를 발생시킵니다.
	void LoseCargoAt(int32 CargoIndex);

	// 파괴됐거나 유효하지 않은 적재물 Actor 참조를 정리합니다.
	void CleanupInvalidCargoActors();

	// 개발 빌드에서 앵커 손실 범위와 적재물 위치를 표시합니다.
	void DrawCargoDebug() const;

	// BeginPlay에서 캐시된 적재 설정입니다.
	FPGCargoSettings CachedCargoSettings;

	// 추가 적재 대기와 시작 인트로 낙하 연출에 사용하는 비물리 메시입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ScriptedCargoMeshComponent;

	// 현재 월드에 살아 있는 물리 적재물 Actor 목록입니다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<APGPhysicsCargoActor>> CargoActors;

	// 추가 적재 대기 종료 후 스폰할 적재물 타입입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> PendingCargoType;

	// 시작 인트로 종료 후 물리 Actor로 전환할 적재물 타입입니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> PendingInitialCargoType;

	// 현재 추가 적재 대기가 진행 중인지 여부입니다.
	bool bHasPendingCargoDrop = false;

	// 시작 적재물 인트로가 진행 중인지 여부입니다.
	bool bIsInitialCargoIntroActive = false;

	// 시작 인트로 종료 후 생성할 적재물 개수입니다.
	int32 PendingInitialCargoCount = 0;

	// 시작 인트로 잠금이 끝날 때까지 남은 시간입니다.
	float InitialCargoIntroRemainingTime = 0.0f;

	// 첫 적재물 투입 직후 손실 판정을 잠시 늦추는 안정화 시간입니다.
	float InitialCargoSettleRemainingTime = 0.0f;

	// 추가 적재 대기가 끝나기까지 남은 시간입니다.
	float CargoDropHoldRemainingTime = 0.0f;

	// 물리 흔들림 계산에 사용하는 경과 시간입니다.
	float CargoShakeElapsedTime = 0.0f;

	// 직전 프레임 CargoStack 앵커 위치입니다.
	FVector PreviousAnchorLocation = FVector::ZeroVector;

	// 현재 프레임 CargoStack 앵커 속도입니다.
	FVector AnchorVelocity = FVector::ZeroVector;

	// 라인 점프/착지 직후 상대속도 보정이 남아 있는 시간입니다.
	float LaneJumpCargoAssistRemainingTime = 0.0f;

	// 직전 프레임에 라인 이동 중이었는지 여부입니다.
	bool bWasChangingLaneLastFrame = false;

	// 앵커 속도 계산을 위한 이전 위치가 준비되었는지 여부입니다.
	bool bHasPreviousAnchorLocation = false;
};
