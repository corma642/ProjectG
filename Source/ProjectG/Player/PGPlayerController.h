// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PGPlayerController.generated.h"

/**
 * 모바일 터치/자이로 입력을 러너 캐릭터로 전달하는 컨트롤러
 */
UCLASS()
class PROJECTG_API APGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APGPlayerController();

	// 매 프레임 자이로 입력을 읽어 러너 캐릭터의 중심 입력으로 전달한다.
	virtual void PlayerTick(float DeltaTime) override;

	// 모바일 터치와 에디터 테스트용 키보드 입력을 바인딩한다.
	virtual void SetupInputComponent() override;

private:
	// 화면 좌/우 터치 위치를 라인 이동 방향으로 변환한다.
	void HandleTouchPressed(ETouchIndex::Type FingerIndex, FVector TouchLocation);

	// 왼쪽 인접 라인으로 이동을 요청한다.
	void MoveLeft();

	// 오른쪽 인접 라인으로 이동을 요청한다.
	void MoveRight();

	// 에디터 테스트용 왼쪽 중심 입력을 시작한다.
	void StartKeyboardBalanceLeft();

	// 에디터 테스트용 왼쪽 중심 입력을 종료한다.
	void StopKeyboardBalanceLeft();

	// 에디터 테스트용 오른쪽 중심 입력을 시작한다.
	void StartKeyboardBalanceRight();

	// 에디터 테스트용 오른쪽 중심 입력을 종료한다.
	void StopKeyboardBalanceRight();

	// 현재 기기 기울기를 새로운 자이로 기준값으로 보정한다.
	void RequestGyroCalibration();

	// 현재 Pawn이 러너 캐릭터일 때 라인 이동 요청을 전달한다.
	void SendLaneChange(int32 Direction);

	// 자이로 값을 읽고 보정/정규화한 뒤 러너 캐릭터에 전달한다.
	void UpdateGyroInput(float DeltaTime);

	// 원본 자이로 기울기 값을 -1~1 범위의 중심 입력으로 변환한다.
	float NormalizeTiltInput(float RawTilt) const;

	// 마지막으로 읽은 보정 전 자이로 좌우 기울기 값
	float LastRawTilt = 0.0f;

	// 현재 적용 중인 자이로 기준 기울기 값
	float CalibrationTilt = 0.0f;

	// 보간 캘리브레이션이 도달해야 할 목표 기준값
	float TargetCalibrationTilt = 0.0f;

	// 에디터에서 자이로 없이 중심 입력을 테스트하기 위한 키보드 값
	float KeyboardBalanceInput = 0.0f;

	// 최초 자이로 기준값이 설정됐는지 여부
	bool bHasCalibration = false;

	// 캘리브레이션 기준값을 부드럽게 보간 중인지 여부
	bool bIsCalibrating = false;
};
