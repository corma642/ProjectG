// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Settings/PGGameSettings.h"
#include "PGPlayerController.generated.h"

class APGRunnerCharacter;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Enhanced Input 기반 모바일 터치/자이로 입력을 러너 캐릭터로 전달하는 컨트롤러
 */
UCLASS()
class PROJECTG_API APGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APGPlayerController();

	// 로컬 플레이어에 Sprint 1 기본 입력 매핑 컨텍스트를 등록한다.
	virtual void BeginPlay() override;

	// 등록한 입력 매핑 컨텍스트를 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 새 Pawn을 소유할 때 러너 캐릭터 참조를 캐시한다.
	virtual void OnPossess(APawn* InPawn) override;

	// Pawn 소유가 해제되면 캐시된 러너 캐릭터 참조를 비운다.
	virtual void OnUnPossess() override;

	// 매 프레임 자이로 입력을 읽어 러너 캐릭터의 중심 입력으로 전달한다.
	virtual void PlayerTick(float DeltaTime) override;

	// Enhanced Input 액션을 모바일 터치/에디터 키보드 검증 입력에 바인딩한다.
	virtual void SetupInputComponent() override;

private:
	// 입력 에셋이 지정되지 않은 경우 C++ 기본 액션과 매핑 컨텍스트를 생성한다.
	void EnsureInputAssets();

	// Project Settings의 입력/디버그 관련 설정을 시작 시 한 번 캐시한다.
	void ApplyGameSettings();

	// 현재 Pawn이 러너 캐릭터라면 약한 참조로 캐시한다.
	void CacheRunnerPawn(APawn* InPawn);

	// 실제 등록에 사용할 입력 매핑 컨텍스트를 반환한다.
	UInputMappingContext* GetResolvedMappingContext() const;

	// 블루프린트에서 지정한 Enhanced Input 에셋 세트가 모두 준비됐는지 확인한다.
	bool HasCompleteConfiguredInputAssets() const;

	// 실제 바인딩에 사용할 터치 라인 이동 액션을 반환한다.
	UInputAction* GetResolvedTouchLaneAction() const;

	// 실제 바인딩에 사용할 왼쪽 라인 이동 액션을 반환한다.
	UInputAction* GetResolvedLaneLeftAction() const;

	// 실제 바인딩에 사용할 오른쪽 라인 이동 액션을 반환한다.
	UInputAction* GetResolvedLaneRightAction() const;

	// 실제 바인딩에 사용할 왼쪽 중심 입력 액션을 반환한다.
	UInputAction* GetResolvedBalanceLeftAction() const;

	// 실제 바인딩에 사용할 오른쪽 중심 입력 액션을 반환한다.
	UInputAction* GetResolvedBalanceRightAction() const;

	// 실제 바인딩에 사용할 자이로 캘리브레이션 액션을 반환한다.
	UInputAction* GetResolvedGyroCalibrationAction() const;

	// 화면 좌/우 터치 위치를 라인 이동 방향으로 변환한다.
	void HandleTouchLaneStarted(const FInputActionValue& ActionValue);

	// 왼쪽 인접 라인으로 이동을 요청한다.
	void HandleLaneLeftStarted(const FInputActionValue& ActionValue);

	// 오른쪽 인접 라인으로 이동을 요청한다.
	void HandleLaneRightStarted(const FInputActionValue& ActionValue);

	// 에디터 테스트용 왼쪽 중심 입력을 시작한다.
	void HandleBalanceLeftStarted(const FInputActionValue& ActionValue);

	// 에디터 테스트용 왼쪽 중심 입력을 종료한다.
	void HandleBalanceLeftCompleted(const FInputActionValue& ActionValue);

	// 에디터 테스트용 오른쪽 중심 입력을 시작한다.
	void HandleBalanceRightStarted(const FInputActionValue& ActionValue);

	// 에디터 테스트용 오른쪽 중심 입력을 종료한다.
	void HandleBalanceRightCompleted(const FInputActionValue& ActionValue);

	// 현재 기기 기울기를 새로운 자이로 기준값으로 보정한다.
	void HandleGyroCalibrationStarted(const FInputActionValue& ActionValue);

	// 현재 Pawn이 러너 캐릭터일 때 라인 이동 요청을 전달한다.
	void SendLaneChange(int32 Direction);

	// 자이로 값을 읽고 보정/정규화한 뒤 러너 캐릭터에 전달한다.
	void UpdateGyroInput(float DeltaTime);

	// 원본 자이로 기울기 값을 -1~1 범위의 중심 입력으로 변환한다.
	float NormalizeTiltInput(float RawTilt) const;

	// 기본으로 등록할 입력 매핑 컨텍스트. 비워두면 C++ 기본 매핑을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// 화면 터치 시작을 감지해 좌/우 라인 이동으로 변환하는 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> TouchLaneAction;

	// 왼쪽 라인 이동 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LaneLeftAction;

	// 오른쪽 라인 이동 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LaneRightAction;

	// 에디터 검증용 왼쪽 중심 입력 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> BalanceLeftAction;

	// 에디터 검증용 오른쪽 중심 입력 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> BalanceRightAction;

	// 자이로 캘리브레이션 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> GyroCalibrationAction;

	// 입력 매핑 컨텍스트 등록 우선순위
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	int32 DefaultMappingPriority = 0;

	// 에셋이 없을 때 사용하는 C++ 기본 입력 매핑 컨텍스트
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> NativeMappingContext;

	// 에셋이 없을 때 사용하는 C++ 기본 터치 액션
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NativeTouchLaneAction;

	// 에셋이 없을 때 사용하는 C++ 기본 왼쪽 라인 이동 액션
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NativeLaneLeftAction;

	// 에셋이 없을 때 사용하는 C++ 기본 오른쪽 라인 이동 액션
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NativeLaneRightAction;

	// 에셋이 없을 때 사용하는 C++ 기본 왼쪽 중심 입력 액션
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NativeBalanceLeftAction;

	// 에셋이 없을 때 사용하는 C++ 기본 오른쪽 중심 입력 액션
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NativeBalanceRightAction;

	// 에셋이 없을 때 사용하는 C++ 기본 캘리브레이션 액션
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NativeGyroCalibrationAction;

	// 마지막으로 읽은 보정 전 자이로 좌우 기울기 값
	float LastRawTilt = 0.0f;

	// 현재 적용 중인 자이로 기준 기울기 값
	float CalibrationTilt = 0.0f;

	// 보간 캘리브레이션이 도달해야 할 목표 기준값
	float TargetCalibrationTilt = 0.0f;

	// 에디터에서 자이로 없이 중심 입력을 테스트하기 위한 키보드 값
	float KeyboardBalanceInput = 0.0f;

	// BeginPlay에서 캐시한 자이로 중심 제어 설정
	FPGGyroSettings CachedGyroSettings;

	// BeginPlay에서 캐시한 에디터 디버그 설정
	FPGDebugSettings CachedDebugSettings;

	// 매 프레임 Pawn Cast를 반복하지 않기 위한 러너 캐릭터 참조
	TWeakObjectPtr<APGRunnerCharacter> CachedRunnerCharacter;

	// 최초 자이로 기준값이 설정됐는지 여부
	bool bHasCalibration = false;

	// 캘리브레이션 기준값을 부드럽게 보간 중인지 여부
	bool bIsCalibrating = false;

	// 에디터 왼쪽 중심 입력 키가 눌려 있는지 여부
	bool bIsKeyboardBalanceLeftHeld = false;

	// 에디터 오른쪽 중심 입력 키가 눌려 있는지 여부
	bool bIsKeyboardBalanceRightHeld = false;
};
