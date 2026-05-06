// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PGGameModeBase.generated.h"

/**
 * 현재 러너 플레이가 어느 단계에 있는지 나타내는 런 상태입니다.
 */
UENUM(BlueprintType)
enum class EPGRunState : uint8
{
	Home,
	Countdown,
	Running,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunCountdownStartedSignature, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPGRunStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunFailedSignature, FName, FailureReason);

/**
 * Go Deliver!의 기본 Pawn/Controller 연결과 홈, 카운트다운, 러닝, 실패 상태를 관리하는 게임모드입니다.
 */
UCLASS()
class PROJECTG_API APGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	APGGameModeBase();

	// 게임 시작 시 홈 상태를 준비합니다.
	virtual void BeginPlay() override;

	// 홈 화면에서 START DELIVERY를 눌렀을 때 카운트다운을 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	void RequestStartDelivery();

	// 적재물 손실, 추락 등으로 런 실패를 요청합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	void RequestRunFailed(FName FailureReason);

	// 현재 러너 플레이 상태입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	EPGRunState GetRunState() const { return RunState; }

	// 카운트다운 시작을 홈 UI, 카운트다운 UI, 러너 시스템에 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunCountdownStartedSignature OnRunCountdownStarted;

	// 실제 러닝 시작을 러너, 적재물, 이벤트, 점수 시스템에 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunStartedSignature OnRunStarted;

	// 런 실패 확정 시 결과 UI와 관련 시스템에 전달합니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunFailedSignature OnRunFailed;

private:
	void FinishRunCountdown();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run", meta = (AllowPrivateAccess = "true"))
	EPGRunState RunState = EPGRunState::Home;

	FTimerHandle RunCountdownTimerHandle;
};
