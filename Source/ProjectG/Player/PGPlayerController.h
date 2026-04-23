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

	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	void HandleTouchPressed(ETouchIndex::Type FingerIndex, FVector TouchLocation);
	void MoveLeft();
	void MoveRight();
	void StartKeyboardBalanceLeft();
	void StopKeyboardBalanceLeft();
	void StartKeyboardBalanceRight();
	void StopKeyboardBalanceRight();
	void RequestGyroCalibration();
	void SendLaneChange(int32 Direction);
	void UpdateGyroInput(float DeltaTime);
	float NormalizeTiltInput(float RawTilt) const;

	float LastRawTilt = 0.0f;
	float CalibrationTilt = 0.0f;
	float TargetCalibrationTilt = 0.0f;
	float KeyboardBalanceInput = 0.0f;
	bool bHasCalibration = false;
	bool bIsCalibrating = false;
};
