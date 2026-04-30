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
class USpringArmComponent;

/**
 * 자동 전진, 3라인 이동, 중심 보정, 바구니 적재 반응을 연결하는 러너 캐릭터입니다.
 * 실제 라인 이동은 전용 MovementComponent가 담당하고, 계란 물리는 CargoStackComponent가 담당합니다.
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

	// 물리 계란이 손실됐을 때 런 실패를 요청한다.
	UFUNCTION()
	void HandleCargoLost(int32 CargoIndex);

	// 런 실패 이벤트를 받으면 자동 전진과 입력 반응을 정지한다.
	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	// 시작 적재물 인트로 동안 라인/밸런스 입력만 잠근다.
	UFUNCTION()
	void HandleInitialCargoIntroStarted();

	// 시작 적재물 인트로가 끝나면 플레이어 입력을 다시 허용한다.
	UFUNCTION()
	void HandleInitialCargoIntroFinished();

protected:
	// 3라인 이동 상태와 캡슐 lateral 이동을 담당하는 러너 전용 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	TObjectPtr<UPGRunnerMovementComponent> RunnerMovementComponent;

	// 머리 바구니와 물리 계란 적재 상태를 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	// 바람, 얼음 길, 계란 추가 요청 같은 환경 이벤트 상태를 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment")
	TObjectPtr<UPGRunnerEnvironmentComponent> RunnerEnvironmentComponent;

	// 캐릭터를 뒤쪽 위에서 따라가는 추적 카메라 암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 플레이어가 실제로 바라보는 추적 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	// Project Settings의 Go Deliver 설정값을 캐시하고 컴포넌트 기본값에 반영한다.
	void ApplyGameSettings();

	// 러너 기준 전방 축으로 매 프레임 자동 전진 입력을 넣는다.
	void UpdateForwardMovement();

	// 시작 높이 기준 추락 실패 조건을 확인하고 GameMode 실패 흐름으로 전달한다.
	void CheckFallFailure();

	// 직선 주행 중 자동으로 발생하는 좌우 중심 흔들림을 갱신한다.
	void UpdateBaseInstability(float DeltaSeconds);

	// 기본 흔들림, 플레이어 입력, 환경 효과를 합산한 최종 중심 입력값을 반환한다.
	float GetEffectiveBalanceInput() const;

	// 중심 입력값을 캐릭터 메시의 좌우 기울기 연출로 변환한다.
	void UpdateBodyLean(float DeltaSeconds);

	// 디버그 옵션이 켜져 있을 때 현재 중심 입력 구성을 화면에 표시한다.
	void DrawBalanceDebug() const;

	// 디버그 옵션이 켜져 있을 때 러너 기준 3라인 위치를 월드에 표시한다.
	void DrawDebugLanes() const;

	// Skeletal Mesh 상대 높이를 보정한다. 현재 물리 계란 구조에서는 기본적으로 0을 적용한다.
	void SetMeshJumpOffset(float JumpOffset);

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
	FPGDebugSettings CachedDebugSettings;

	// BeginPlay에서 캐시한 물리 계란/바구니 설정
	FPGCargoSettings CachedCargoSettings;

	// BeginPlay에서 캐시한 바람/얼음 길 환경 이벤트 설정
	FPGEnvironmentSettings CachedEnvironmentSettings;

	// 현재 플레이어가 입력 중인 좌우 중심 보정값
	float BalanceInput = 0.0f;

	// 직선 주행 중 자동으로 발생하는 기본 좌우 중심 흔들림 입력값
	float BaseInstabilityInput = 0.0f;

	// 기본 흔들림 위상 계산에 사용하는 런 진행 시간
	float BalanceElapsedTime = 0.0f;

	// 현재 러너가 조작 가능한 런 진행 상태인지 여부
	bool bIsRunActive = true;

	// 시작 적재물 인트로 중 플레이어 조작 입력을 막는 상태. 자동 전진은 유지한다.
	bool bIsPlayerInputLockedByCargoIntro = false;

	// 추락 실패 판정 기준이 되는 시작 월드 Z 위치
	float StartLocationZ = 0.0f;

	// 메시 연출에 적용 중인 현재 롤 각도
	float CurrentBodyRoll = 0.0f;

	// 몸 기울기 연출을 더하기 위한 Skeletal Mesh 초기 상대 회전
	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;

	// 점프 연출을 더하기 위한 Skeletal Mesh 초기 상대 위치
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
};
