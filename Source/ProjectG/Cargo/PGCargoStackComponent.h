// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGCargoStackComponent.generated.h"

class APGPhysicsCargoActor;
class UPGCargoTypeDefinition;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGCargoLostSignature, int32, CargoIndex);

/**
 * 플레이어에 부착된 물리 계란 스폰/손실 판정 앵커입니다.
 * 바구니 메시와 충돌은 블루프린트가 담당하며, 이 컴포넌트는 계란 생성과 위치 기반 실패 판정만 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 계란이 앵커 기준 허용 범위를 벗어나 손실됐을 때 호출되는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Cargo")
	FPGCargoLostSignature OnCargoLost;

	// Project Settings의 적재 설정을 적용하고 시작 계란을 생성한다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings);

	// 중심 입력과 프리뷰 타이머, 계란 물리 보조 힘, 손실 판정을 갱신한다.
	void UpdateCargoReaction(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane);

	// 기본 계란 타입으로 새 계란 추가 프리뷰를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargo();

	// 지정한 계란 타입으로 새 계란 추가 프리뷰를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool AddCargoByType(UPGCargoTypeDefinition* CargoType);

	// 가장 마지막 계란을 손실 처리한다. 디버그/테스트용으로 사용한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Cargo")
	bool RemoveCargo();

	// 현재 살아 있는 계란 수
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	int32 GetCurrentCargoCount() const { return CargoActors.Num(); }

private:
	// 시작 계란을 물리 Actor로 즉시 배치한다.
	void SpawnInitialCargo();

	// 계란 추가 요청을 플레이어를 따라가는 프리뷰 상태로 전환한다.
	bool StartCargoPreview(UPGCargoTypeDefinition* CargoType);

	// 프리뷰 시간이 끝났을 때 현재 앵커 위치 기준으로 실제 물리 계란을 낙하시킨다.
	void FinishCargoPreview();

	// 지정한 Transform에 물리 계란 Actor를 생성한다.
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);

	// DefaultCargoType을 동기 로드해 반환한다.
	UPGCargoTypeDefinition* ResolveDefaultCargoType() const;

	// 플레이어에 붙은 CargoStackComponent 기준 상대 위치를 월드 Transform으로 변환한다.
	FTransform MakeCargoWorldTransform(const FVector& LocalOffset, int32 CargoIndex = 0) const;

	// 살아 있는 계란에 중심 흔들림 보조 힘을 적용한다.
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);

	// 계란이 현재 플레이어 앵커 기준 허용 범위를 벗어났는지 확인한다.
	void CheckCargoLoss();

	// 지정 계란을 제거하고 손실 이벤트를 발생시킨다.
	void LoseCargoAt(int32 CargoIndex);

	// 파괴됐거나 유효하지 않은 계란 Actor 참조를 정리한다.
	void CleanupInvalidCargoActors();

	// 개발 빌드에서 앵커 손실 범위와 계란 위치를 표시한다.
	void DrawCargoDebug() const;

	// BeginPlay에서 캐시한 적재 설정
	FPGCargoSettings CachedCargoSettings;

	// 새 계란이 떨어지기 전 위치를 보여주는 비물리 프리뷰 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PreviewCargoMeshComponent;

	// 현재 월드에 살아 있는 물리 계란 Actor 목록
	UPROPERTY(Transient)
	TArray<TObjectPtr<APGPhysicsCargoActor>> CargoActors;

	// 프리뷰 종료 후 스폰할 계란 타입
	UPROPERTY(Transient)
	TObjectPtr<UPGCargoTypeDefinition> PendingCargoType;

	// 현재 새 계란 프리뷰가 진행 중인지 여부
	bool bHasPendingCargoDrop = false;

	// 새 계란 프리뷰가 끝나기까지 남은 시간
	float CargoPreviewRemainingTime = 0.0f;

	// 물리 흔들림 계산에 사용하는 경과 시간
	float CargoShakeElapsedTime = 0.0f;
};
