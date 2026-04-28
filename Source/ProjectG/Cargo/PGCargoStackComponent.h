// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGCargoStackComponent.generated.h"

class APGPhysicsCargoActor;
class UPGCargoTypeDefinition;
class USkeletalMeshComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGCargoLostSignature, int32, CargoIndex);

/**
 * 러너 머리의 바구니와 물리 계란 적재 상태를 관리하는 컴포넌트입니다.
 * 바구니는 캐릭터 소켓에 부착된 메시 콜리전을 사용하고, 계란은 별도 물리 Actor로 스폰됩니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoStackComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPGCargoStackComponent();

	// 계란이 바구니 밖으로 떨어져 손실됐을 때 호출되는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Cargo")
	FPGCargoLostSignature OnCargoLost;

	// Project Settings의 적재 설정을 적용하고 바구니/초기 계란을 준비한다.
	void InitializeCargoStack(const FPGCargoSettings& InCargoSettings, USkeletalMeshComponent* RunnerMesh);

	// 중심 입력과 라인 점프 상태를 바구니 기울기, 계란 물리 보조 힘, 손실 판정에 반영한다.
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

	// 현재 바구니에 적용 중인 기울기 각도
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	float GetCurrentSwayRoll() const { return CurrentBasketRoll; }

	// 바구니 메시 컴포넌트. 에디터에서 메시 콜리전 상태를 확인할 때 사용한다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Cargo")
	UStaticMeshComponent* GetBasketMeshComponent() const { return BasketMeshComponent; }

private:
	// 바구니를 머리 소켓 또는 fallback 위치에 부착한다.
	void AttachBasketRoot(USkeletalMeshComponent* RunnerMesh);

	// 바구니 메시와 충돌 설정을 Project Settings 기준으로 적용한다.
	void ApplyBasketSettings();

	// 시작 계란을 물리 Actor로 즉시 배치한다.
	void SpawnInitialCargo();

	// 계란 추가 요청을 홀로그램 프리뷰 상태로 전환한다.
	bool StartCargoPreview(UPGCargoTypeDefinition* CargoType);

	// 프리뷰 시간이 끝났을 때 실제 물리 계란을 낙하시킨다.
	void FinishCargoPreview();

	// 지정한 Transform에 물리 계란 Actor를 생성한다.
	APGPhysicsCargoActor* SpawnPhysicsCargo(UPGCargoTypeDefinition* CargoType, const FTransform& SpawnTransform);

	// DefaultCargoType을 동기 로드해 반환한다.
	UPGCargoTypeDefinition* ResolveDefaultCargoType() const;

	// 바구니 기준 상대 위치를 월드 Transform으로 변환한다.
	FTransform MakeCargoWorldTransform(const FVector& LocalOffset, int32 CargoIndex = 0) const;

	// 바구니 흔들림과 라인 점프 시각 오프셋을 메시 Transform에 반영한다.
	void UpdateBasketTransform(float DeltaSeconds, float BalanceInput, float VisualJumpOffset, bool bIsChangingLane);

	// 살아 있는 계란에 중심 흔들림 보조 힘을 적용한다.
	void ApplyCargoPhysicsForces(float DeltaSeconds, float BalanceInput);

	// 계란이 바구니 기준 허용 범위를 벗어났는지 확인한다.
	void CheckCargoLoss();

	// 지정 계란을 제거하고 손실 이벤트를 발생시킨다.
	void LoseCargoAt(int32 CargoIndex);

	// 파괴됐거나 유효하지 않은 계란 Actor 참조를 정리한다.
	void CleanupInvalidCargoActors();

	// 개발 빌드에서 바구니 손실 범위와 계란 위치를 표시한다.
	void DrawCargoDebug() const;

	// BeginPlay에서 캐시한 적재 설정
	FPGCargoSettings CachedCargoSettings;

	// 캐릭터 머리를 따라가는 바구니 메시. 메시 자체 Simple Collision으로 계란을 받는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BasketMeshComponent;

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

	// 바구니에 현재 적용 중인 롤 각도
	float CurrentBasketRoll = 0.0f;

	// 물리 흔들림 계산에 사용하는 경과 시간
	float CargoShakeElapsedTime = 0.0f;
};
