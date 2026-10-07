// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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
 * 모바일 터치 드래그/플릭 입력과 에디터 테스트 입력을 러너 캐릭터에 전달하는 컨트롤러입니다.
 */
UCLASS()
class PROJECTG_API APGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Enhanced Input과 Home/HUD/오디오/피드백 소유 컴포넌트를 기본 서브오브젝트로 구성합니다. */
	APGPlayerController();

	/** 로컬 플레이어에 기본 입력 매핑과 터치 설정을 적용하고 초기 Pawn 캐시를 준비합니다. */
	virtual void BeginPlay() override;

	/** 등록한 입력 매핑 컨텍스트와 러너 입력 상태를 레벨 종료 전에 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 새 Pawn이 러너면 캐시하고 현재 Q/E 키 상태를 즉시 재적용합니다. */
	virtual void OnPossess(APawn* InPawn) override;

	/** 소유 해제 전에 러너 중심 입력을 중립으로 돌리고 약한 캐시를 비웁니다. */
	virtual void OnUnPossess() override;

	/** Enhanced Input의 점프와 Q/E Started·Completed·Canceled 이벤트를 바인딩합니다. */
	virtual void SetupInputComponent() override;

	/** Android 터치 드래그·플릭을 Balance 입력과 점프로 변환하고 입력을 처리했는지 반환합니다. */
	virtual bool InputTouch(const FInputDeviceId DeviceId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, uint32 TouchpadIndex, const uint64 Timestamp) override;

private:
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	// Project Settings의 터치 조작 설정을 캐시합니다.
	void ApplyGameSettings();

	// 현재 Pawn이 러너 캐릭터라면 약한 참조로 캐시합니다.
	void CacheRunnerPawn(APawn* InPawn);

	// 릴리즈 입력에 필요한 에디터 지정 입력 에셋이 준비됐는지 확인합니다.
	bool HasRequiredInputAssets() const;

	// 데스크톱 점프 키 입력으로 점프를 요청합니다. Android 터치 점프는 플릭 제스처가 담당합니다.
	void HandleJumpStarted(const FInputActionValue& ActionValue);

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

	// 터치 시작 위치와 시간을 기록해 드래그/플릭 분류 기준으로 사용합니다.
	void BeginTouchGesture(uint32 Handle, const FVector2D& TouchLocation);

	// 터치 이동량을 -1~1 중심 보정값으로 변환합니다.
	void UpdateTouchGesture(uint32 Handle, const FVector2D& TouchLocation);

	// 터치 종료 시 짧고 빠른 플릭이면 방향별 점프를 실행하고, 아니면 입력만 해제합니다.
	void EndTouchGesture(uint32 Handle, const FVector2D& TouchLocation);

	// 현재 터치 제스처 상태와 러너 입력을 중립으로 되돌립니다.
	void ResetTouchGesture();

	// 종료된 터치 궤적이 왼쪽/오른쪽/위쪽 플릭인지 판정하고 처리합니다.
	bool TryHandleFlickGesture(const FVector2D& EndLocation, float GestureDurationSeconds);

	// Q/E 상태가 바뀌거나 Pawn을 다시 소유할 때 데스크탑 테스트 입력을 즉시 전달합니다.
	void UpdateKeyboardBalanceInput();

	// 데스크탑 테스트용 Q/E 중심 보정 입력을 -1~1 범위로 반환합니다.
	float GetKeyboardBalanceInput() const;

	// 기본으로 등록할 입력 매핑 컨텍스트. 에디터에서 반드시 지정합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// 데스크톱 테스트용 점프 액션. Android에서는 바인딩하지 않습니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

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

	// BeginPlay에서 캐시한 터치 드래그/플릭 조작 설정
	FPGTouchControlSettings CachedTouchControlSettings;

	// 입력 이벤트마다 Pawn Cast를 반복하지 않기 위한 러너 캐릭터 약한 참조입니다.
	TWeakObjectPtr<APGRunnerCharacter> CachedRunnerCharacter;

	// Q를 누르고 있는 동안 true가 되어 왼쪽 중심 보정을 적용합니다.
	bool bIsBalanceLeftHeld = false;

	// E를 누르고 있는 동안 true가 되어 오른쪽 중심 보정을 적용합니다.
	bool bIsBalanceRightHeld = false;

	// 현재 추적 중인 터치가 있는지 여부
	bool bHasActiveTouchGesture = false;

	// 여러 손가락이 들어와도 첫 터치 하나만 조작 입력으로 사용합니다.
	uint32 ActiveTouchHandle = 0;

	// 터치 시작 화면 위치
	FVector2D TouchStartLocation = FVector2D::ZeroVector;

	// 터치 시작 월드 시간
	float TouchStartTimeSeconds = 0.0f;
};
