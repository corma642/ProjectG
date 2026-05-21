// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Settings/PGGameSettings.h"
#include "PGPlayerController.generated.h"

class APGRunnerCharacter;
class UInputAction;
class UInputMappingContext;
class UPGFPSDisplayComponent;
class UPGHomeFlowComponent;
class UPGMusicComponent;
class UPGRunHUDComponent;
class UPGRunEventFeedbackComponent;
class UPGRunResultFeedbackComponent;
struct FInputActionValue;

/**
 * 모바일 터치/기울임 입력과 에디터 테스트 입력을 러너 캐릭터에 전달하는 컨트롤러입니다.
 */
UCLASS()
class PROJECTG_API APGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APGPlayerController();

	// 로컬 플레이어에 기본 입력 매핑과 프로젝트 설정을 적용합니다.
	virtual void BeginPlay() override;

	// 등록된 입력 매핑 컨텍스트를 정리합니다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 소유한 러너 캐릭터 참조를 갱신합니다.
	virtual void OnPossess(APawn* InPawn) override;

	// 소유 해제 시 러너 캐릭터 참조와 피드백 바인딩을 정리합니다.
	virtual void OnUnPossess() override;

	// 매 프레임 기울임 입력을 계산해 러너 캐릭터에 전달합니다.
	virtual void PlayerTick(float DeltaTime) override;

	// Enhanced Input 액션을 바인딩합니다.
	virtual void SetupInputComponent() override;

private:
	// Project Settings의 자이로 입력 설정을 캐시합니다.
	void ApplyGameSettings();

	// 현재 Pawn이 러너 캐릭터라면 약한 참조로 캐시합니다.
	void CacheRunnerPawn(APawn* InPawn);

	// 릴리즈 입력에 필요한 에디터 지정 입력 에셋이 준비됐는지 확인합니다.
	bool HasRequiredInputAssets() const;

	// 화면 터치/Space 입력으로 점프를 요청합니다.
	void HandleJumpStarted(const FInputActionValue& ActionValue);

	// 현재 기기 기울기를 새로운 자이로 기준값으로 보정합니다.
	void HandleGyroCalibrationStarted(const FInputActionValue& ActionValue);

	// Q를 누르는 동안 왼쪽 중심 보정 입력을 유지합니다.
	void HandleBalanceLeftStarted(const FInputActionValue& ActionValue);

	// Q에서 손을 떼면 왼쪽 중심 보정 입력을 해제합니다.
	void HandleBalanceLeftCompleted(const FInputActionValue& ActionValue);

	// E를 누르는 동안 오른쪽 중심 보정 입력을 유지합니다.
	void HandleBalanceRightStarted(const FInputActionValue& ActionValue);

	// E에서 손을 떼면 오른쪽 중심 보정 입력을 해제합니다.
	void HandleBalanceRightCompleted(const FInputActionValue& ActionValue);

	// 현재 Pawn이 러너 캐릭터라면 점프 요청을 전달합니다.
	void SendJumpRequest();

	// 자이로 값을 중심 입력으로 변환해 전달합니다.
	void UpdateGyroInput(float DeltaTime);

	// Raw 기울임 값을 -1~1 범위의 중심 입력으로 변환합니다.
	float NormalizeTiltInput(float RawTilt) const;

	// 데스크탑 테스트용 Q/E 중심 보정 입력을 -1~1 범위로 반환합니다.
	float GetKeyboardBalanceInput() const;

	// 데스크탑에서는 Q/E, 모바일에서는 자이로 값을 최종 중심 입력으로 사용합니다.
	float ResolveFinalBalanceInput(float MotionBalanceInput) const;

	// 기본으로 등록할 입력 매핑 컨텍스트. 에디터에서 반드시 지정합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// 화면 터치/Space 점프 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	// 자이로 캘리브레이션 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> GyroCalibrationAction;

	// 데스크탑 테스트용 왼쪽 중심 보정 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> BalanceLeftAction;

	// 데스크탑 테스트용 오른쪽 중심 보정 액션
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> BalanceRightAction;

	// 입력 매핑 컨텍스트 등록 우선순위
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	int32 DefaultMappingPriority = 0;

	// 전역 런 이벤트의 UI/VFX/SFX를 로컬 플레이어 기준으로 자동 재생하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run Event", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPGRunEventFeedbackComponent> RunEventFeedbackComponent;

	// 런 실패 결과 화면 UI/SFX/CameraShake를 자동 재생하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPGRunResultFeedbackComponent> RunResultFeedbackComponent;

	// 홈/러닝 배경음을 로컬 플레이어 기준으로 자동 재생하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPGMusicComponent> MusicComponent;

	// 홈 화면 표시, 시작 버튼 처리, 카운트다운 UI/카메라 전환을 자동 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPGHomeFlowComponent> HomeFlowComponent;

	// 러닝 중 HUD, Pause, 인게임 Settings 흐름을 자동 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPGRunHUDComponent> RunHUDComponent;

	// Android 프레임 테스트용 FPS 오버레이를 Settings 저장값에 맞춰 표시/숨김 처리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPGFPSDisplayComponent> FPSDisplayComponent;

	// 마지막으로 읽은 Raw 좌우 기울임 값
	float LastRawTilt = 0.0f;

	// 현재 적용 중인 기울임 기준값
	float CalibrationTilt = 0.0f;

	// 보간 캘리브레이션이 도달해야 할 목표 기준값
	float TargetCalibrationTilt = 0.0f;

	// BeginPlay에서 캐시한 자이로 중심 제어 설정
	FPGGyroSettings CachedGyroSettings;

	// 매 프레임 Pawn Cast를 반복하지 않기 위한 러너 캐릭터 참조
	TWeakObjectPtr<APGRunnerCharacter> CachedRunnerCharacter;

	// 최초 자이로 기준값이 설정됐는지 여부
	bool bHasCalibration = false;

	// 캘리브레이션 기준값을 부드럽게 보간 중인지 여부
	bool bIsCalibrating = false;

	// Q를 누르고 있는 동안 true가 되어 왼쪽 중심 보정을 적용합니다.
	bool bIsBalanceLeftHeld = false;

	// E를 누르고 있는 동안 true가 되어 오른쪽 중심 보정을 적용합니다.
	bool bIsBalanceRightHeld = false;
};
