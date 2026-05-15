// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Environment/PGRunnerEnvironmentComponent.h"
#include "GameFramework/Character.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerCharacter.generated.h"

class UCameraComponent;
class UPGCharacterSkinDefinition;
class UPGRunProgressComponent;
class USpringArmComponent;

/**
 * 자동 전진, 3라인 이동, 중심 보정, 적재물 반응을 연결하는 러너 캐릭터입니다.
 * 실제 라인 이동은 전용 MovementComponent가 담당하고, 적재물 물리는 CargoStackComponent가 담당합니다.
 */
UCLASS()
class PROJECTG_API APGRunnerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APGRunnerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 매 프레임 자동 전진, 중심 불안정성, 적재물 물리 반응, 디버그 표시를 갱신한다.
	virtual void Tick(float DeltaSeconds) override;

	// 시작 위치와 Project Settings 값을 캐시하고 런타임 컴포넌트를 초기화한다.
	virtual void BeginPlay() override;

	// 런 중 생성한 Bowl Actor가 레벨 전환/재시작 때 남지 않도록 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 입력 방향(-1: 왼쪽, 1: 오른쪽)에 따라 인접 라인 이동을 요청한다.
	void RequestLaneChange(int32 Direction);

	// 자이로/키보드에서 계산한 플레이어 중심 보정 입력을 캐릭터에 반영한다.
	void SetBalanceInput(float InBalanceInput);

	// 현재 캐릭터가 위치한 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetCurrentLaneIndex() const;

	// 라인 이동 중 목표로 삼는 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetTargetLaneIndex() const;

	// 현재 플레이어가 입력 중인 좌우 중심 보정값
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetBalanceInput() const { return BalanceInput; }

	// 현재 런이 시작된 뒤 경과한 시간입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetRunElapsedTime() const;

	// 런 시작 지점 기준 전방 진행 거리입니다. UI 표시를 위해 미터 단위로 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetRunDistanceMeters() const;

	// 현재 결과 화면에 표시할 임시 점수입니다. 현 단계에서는 진행 거리(m)를 정수화합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetRunScore() const;

	// 현재 런에서 획득한 영수증 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetRunReceipts() const;

	// 현재 런에서 배달 완료한 적재물 개수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetDeliveredCargoCount() const;

	// 적재물 자동 연출/환경 이벤트 시스템이 현재 러너의 적재 상태에 접근할 때 사용합니다.
	UPGCargoStackComponent* GetCargoStackComponent() const { return CargoStackComponent; }

	// HUD가 점수, 거리, 영수증 변화를 구독할 때 사용하는 진행 컴포넌트입니다.
	UPGRunProgressComponent* GetRunProgressComponent() const { return RunProgressComponent; }

	// 상점/저장 시스템에서 선택한 캐릭터 스킨을 러너 시각 메시로 적용합니다.
	bool ApplyCharacterSkin(const UPGCharacterSkinDefinition* SkinDefinition);

	// 현재 저장된 장착 캐릭터 스킨을 러너에 적용합니다.
	void ApplyEquippedCharacterSkin();

	// 물리 적재물이 손실됐을 때 런 실패를 요청한다.
	UFUNCTION()
	void HandleCargoLost(int32 CargoIndex);

	// 적재물 배달 완료 시 점수 보상을 반영합니다.
	void HandleCargoDelivered(int32 CargoInstanceId, int32 Score, int32 Receipts, FVector DeliveryLocation);

	// 런 실패 이벤트를 받으면 자동 전진과 입력 반응을 정지한다.
	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunStarted();

protected:
	// 3라인 이동 상태와 캡슐 lateral 이동을 담당하는 러너 전용 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	TObjectPtr<UPGRunnerMovementComponent> RunnerMovementComponent;

	// 런 시작 시 머리 소켓에 스폰/부착할 바구니 Actor 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	TSubclassOf<AActor> BowlActorClass;

	// 바구니 부착 소켓 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	FName BowlSocketName = FName(TEXT("s_HeadCargoSocket"));

	// 물리 적재물 생성/대기/손실 상태를 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	// 바람, 눈바람, 적재물 추가 요청 같은 환경 이벤트 상태를 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment")
	TObjectPtr<UPGRunnerEnvironmentComponent> RunnerEnvironmentComponent;

	// 런 중 진행 거리와 점수를 추적하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run Progress")
	TObjectPtr<UPGRunProgressComponent> RunProgressComponent;

	// 캐릭터를 뒤쪽 위에서 따라가는 추적 카메라 암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 플레이어가 실제로 바라보는 추적 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	// Project Settings의 Go Deliver 설정값을 캐시하고 컴포넌트 기본값에 반영한다.
	void ApplyGameSettings();

	// 카운트다운 시작 시 현재 캐릭터 메시의 BowlSocketName에 바구니 Actor를 1회 부착한다.
	void AttachBowlForRun();

	// 런 실패/레벨 종료 시 런타임 바구니 Actor를 제거한다.
	void DetachAndDestroyBowl();

	// 러너 기준 전방 축으로 매 프레임 자동 전진 입력을 넣는다.
	void UpdateForwardMovement();

	// 시작 높이 기준 추락 실패 조건을 확인하고 GameMode 실패 흐름으로 전달한다.
	void CheckFallFailure();

	// 직선 주행 중 자동으로 발생하는 좌우 중심 흔들림을 갱신한다.
	void UpdateBaseInstability(float DeltaSeconds);

	// 기본 흔들림, 플레이어 입력, 환경 효과를 합산한 최종 중심 입력값을 반환한다.
	float GetEffectiveBalanceInput() const;

	// 적재물 물리 흔들림에 사용할 중심 입력값을 반환합니다. 눈바람 중에는 플레이어 입력 기여분만 추가 증폭합니다.
	float GetCargoBalanceInputForPhysics() const;

	// 중심 입력값을 캐릭터 메시의 좌우 기울기 연출로 변환한다.
	void UpdateBodyLean(float DeltaSeconds);

	// 디버그 옵션이 켜져 있을 때 현재 중심 입력 구성을 화면에 표시한다.
	void DrawBalanceDebug() const;

	// 디버그 옵션이 켜져 있을 때 러너 기준 3라인 위치를 월드에 표시한다.
	// BeginPlay에서 캐시한 자동 전진/몸 기울기 설정
	FPGRunnerSettings CachedRunnerSettings;

	// BeginPlay에서 캐시한 기본 중심 불안정성 설정
	FPGBalanceSettings CachedBalanceSettings;

	// BeginPlay에서 캐시한 3라인 이동 설정
	FPGLaneSettings CachedLaneSettings;

	// BeginPlay에서 캐시한 자이로 중심 제어 설정
	FPGGyroSettings CachedGyroSettings;

	// BeginPlay에서 캐시한 카메라 설정
	FPGCameraSettings CachedCameraSettings;

	// BeginPlay에서 캐시한 에디터 디버그 설정
	// BeginPlay에서 캐시한 물리 적재물 설정
	FPGCargoSettings CachedCargoSettings;

	// BeginPlay에서 캐시한 바람/눈바람 환경 이벤트 설정
	FPGEnvironmentSettings CachedEnvironmentSettings;

	// 현재 플레이어가 입력 중인 좌우 중심 보정값
	float BalanceInput = 0.0f;

	// 직선 주행 중 자동으로 발생하는 기본 좌우 중심 흔들림 입력값
	float BaseInstabilityInput = 0.0f;

	// 기본 흔들림 위상 계산에 사용하는 런 진행 시간
	float BalanceElapsedTime = 0.0f;

	// 현재 러너가 조작 가능한 런 진행 상태인지 여부
	bool bIsRunActive = false;

	// 시작 적재물 투입 전 플레이어 조작 입력을 막는 상태. 자동 전진은 유지한다.
	bool bIsRunCountdownActive = false;

	bool bIsGameplayInputLocked = true;

	// 추락 실패 판정 기준이 되는 시작 월드 Z 위치
	float StartLocationZ = 0.0f;

	// 런 진행 거리 계산 기준이 되는 시작 월드 위치
	FVector RunStartLocation = FVector::ZeroVector;

	// 런 진행 거리 계산에 사용하는 시작 시점 전방 방향
	FVector RunStartForwardVector = FVector::ForwardVector;

	// 런 경과 시간 계산에 사용하는 월드 시간
	float RunStartTimeSeconds = 0.0f;

	// 메시 연출에 적용 중인 현재 롤 각도
	float CurrentBodyRoll = 0.0f;

	// 몸 기울기 연출을 더하기 위한 Skeletal Mesh 초기 상대 회전
	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<AActor> AttachedBowlActor;
};
