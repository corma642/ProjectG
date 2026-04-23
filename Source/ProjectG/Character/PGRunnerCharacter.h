// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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
	APGRunnerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	void RequestLaneChange(int32 Direction);
	void SetBalanceInput(float InBalanceInput);

	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetCurrentLaneIndex() const { return CurrentLaneIndex; }

	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetTargetLaneIndex() const { return TargetLaneIndex; }

	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetBalanceInput() const { return BalanceInput; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	void ApplyGameSettings();
	void UpdateForwardMovement();
	void UpdateLaneMovement(float DeltaSeconds);
	void UpdateBodyLean(float DeltaSeconds);
	void DrawDebugLanes() const;
	void StartLaneChange(int32 NewLaneIndex);
	void ConsumeBufferedLaneInput();
	int32 GetCenterLaneIndex() const;
	int32 ClampLaneIndex(int32 LaneIndex) const;
	float GetLaneOffset(int32 LaneIndex) const;

	int32 CurrentLaneIndex = 0;
	int32 TargetLaneIndex = 0;
	int32 BufferedLaneDirection = 0;

	float CenterLaneY = 0.0f;
	float LaneStartY = 0.0f;
	float LaneTargetY = 0.0f;
	float LaneStartZ = 0.0f;
	float LaneMoveElapsed = 0.0f;
	float BufferedLaneInputRemainingTime = 0.0f;
	float LandingStabilizeRemainingTime = 0.0f;
	float BalanceInput = 0.0f;
	float CurrentBodyRoll = 0.0f;
	bool bIsChangingLane = false;

	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;
};
