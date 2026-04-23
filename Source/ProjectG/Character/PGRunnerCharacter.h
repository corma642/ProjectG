// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cargo/PGCargoStackComponent.h"
#include "GameFramework/Character.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Go Deliver!의 자동 전진 러너 캐릭터
 */
UCLASS()
class PROJECTG_API APGRunnerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APGRunnerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 매 프레임 자동 전진, 라인 이동, 몸 기울기, 디버그 표시를 갱신한다.
	virtual void Tick(float DeltaSeconds) override;

	// 시작 위치를 기준으로 러너 진행 축, 중앙 라인, 초기 메시 상태를 저장한다.
	virtual void BeginPlay() override;

	// 입력 방향(-1: 왼쪽, 1: 오른쪽)에 따라 인접 라인 이동을 요청한다.
	void RequestLaneChange(int32 Direction);

	// 자이로/키보드에서 계산된 중심 입력값을 캐릭터 기울기 반응에 반영한다.
	void SetBalanceInput(float InBalanceInput);

	// 현재 캐릭터가 위치한 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetCurrentLaneIndex() const;

	// 라인 이동 중 목표로 삼고 있는 라인 인덱스
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetTargetLaneIndex() const;

	// 현재 적용 중인 좌우 중심 입력값
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetBalanceInput() const { return BalanceInput; }

protected:
	// 3라인 이동 상태와 캡슐 lateral 이동을 담당하는 러너 전용 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	TObjectPtr<UPGRunnerMovementComponent> RunnerMovementComponent;

	// 머리 위 적재물 개수와 시각 흔들림을 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	// 캐릭터를 약간 위에서 따라가는 추적 카메라 암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 플레이어가 실제로 바라보는 추적 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	// Project Settings의 Go Deliver 설정값을 시작 시 캐시하고 런타임 컴포넌트에 반영한다.
	void ApplyGameSettings();

	// 러너 기준 전방 축으로 매 프레임 자동 전진 입력을 넣는다.
	void UpdateForwardMovement();

	// 중심 입력값을 캐릭터 메시의 좌우 기울기 연출로 변환한다.
	void UpdateBodyLean(float DeltaSeconds);

	// 디버그 옵션이 켜져 있을 때 러너 기준 3라인 위치를 월드에 표시한다.
	void DrawDebugLanes() const;

	// 캡슐 Z 이동 없이 메시 상대 위치만 올려 라인 점프감을 만든다.
	void SetMeshJumpOffset(float JumpOffset);

	// BeginPlay에서 캐시한 자동 전진/몸 기울기 설정
	FPGRunnerSettings CachedRunnerSettings;

	// BeginPlay에서 캐시한 3라인 이동 설정
	FPGLaneSettings CachedLaneSettings;

	// BeginPlay에서 캐시한 자이로 중심 제어 설정
	FPGGyroSettings CachedGyroSettings;

	// BeginPlay에서 캐시한 카메라 설정
	FPGCameraSettings CachedCameraSettings;

	// BeginPlay에서 캐시한 에디터 디버그 설정
	FPGDebugSettings CachedDebugSettings;

	// BeginPlay에서 캐시한 적재물 스택 설정
	FPGCargoSettings CachedCargoSettings;

	// 현재 좌우 중심 입력값
	float BalanceInput = 0.0f;

	// 메시 연출에 적용 중인 현재 롤 각도
	float CurrentBodyRoll = 0.0f;

	// 몸 기울기 연출을 더하기 전 Skeletal Mesh의 초기 상대 회전
	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;

	// 점프 연출을 더하기 전 Skeletal Mesh의 초기 상대 위치
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
};
