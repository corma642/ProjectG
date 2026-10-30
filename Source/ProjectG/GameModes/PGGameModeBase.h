// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PGGameModeBase.generated.h"

class APGRunnerCharacter;

/**
 * 현재 러너 플레이가 어느 단계에 있는지 나타내는 상태입니다.
 */
UENUM(BlueprintType)
enum class EPGRunState : uint8
{
	Home,
	Countdown,
	Running,
	Failed
};

/** 초 단위 카운트다운이 시작돼 러너와 런 시스템이 준비 상태로 전환될 때 발생합니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunCountdownStartedSignature, float, Duration);
/** 카운트다운이 끝나 점수·난이도·전진 갱신을 시작할 때 발생합니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPGRunStartedSignature);
/** 실패 원인이 확정돼 런타임 시스템을 정지하고 결과 화면을 표시할 때 발생합니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunFailedSignature, FName, FailureReason);

/**
 * Go Deliver!의 기본 Pawn/Controller 연결과 홈, 카운트다운, 러닝, 실패 상태를 관리하는 게임모드입니다.
 */
UCLASS()
class PROJECTG_API APGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** 레벨 배치 러너를 START 시점에 Possess하는 상태 머신 기본값을 구성합니다. */
	APGGameModeBase();

	/** 러너는 레벨에 배치한 BP_Runner를 START 시점에 Possess하므로 기본 Pawn 자동 스폰을 막습니다. */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** 레벨 설정을 검증하고 Home 또는 Retry 진입 상태를 준비한 뒤 초기 delegate 순서를 결정합니다. */
	virtual void BeginPlay() override;

	/** Home 또는 Retry에서 START DELIVERY 카운트다운을 시작합니다. 그 외 상태의 중복 요청은 무시합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	void RequestStartDelivery();

	/** Running 상태에서 적재물 손실·추락 실패를 확정하고 보상 정산 후 OnRunFailed를 발생시킵니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	void RequestRunFailed(FName FailureReason);

	/** 실패 연출 없이 현재 런 보상을 한 번만 정산합니다. 새 정산이 저장되면 true를 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	bool SettleCurrentRunRewards(FName Reason);

	/** Failed 상태에서 결과 화면에 이어하기 버튼을 보여 줄지 반환합니다. 기능이 켜져 있고 이번 런의 이어하기 횟수가 남았을 때 true입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	bool IsContinueOffered() const;

	/** 이어하기 버튼을 누를 수 있는지 반환합니다. IsContinueOffered에 더해 지갑 영수증이 비용 이상일 때 true입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	bool CanContinueRun() const;

	/** 이어하기 1회에 차감하는 영수증 수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	int32 GetContinueReceiptCost() const;

	/** 영수증을 차감하고 실패 지점에서 Retry와 같은 길이의 카운트다운 뒤 런을 재개합니다. 차감·저장에 실패하면 false를 반환하고 상태를 바꾸지 않습니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	bool RequestContinueRun();

	/** 현재 플레이 러너를 반환합니다. Possess 전에는 검증된 레벨 배치 러너를 반환하며 없으면 null입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	APGRunnerCharacter* GetPlayableRunnerCharacter() const;

	/** 현재 Home/Countdown/Running/Failed 상태를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	EPGRunState GetRunState() const { return RunState; }

	/** 현재(또는 마지막) 카운트다운의 초 단위 길이입니다. 카운트다운 도중 늦게 바인딩한 UI가 GameMode 타이머와 같은 길이를 쓰기 위한 값입니다. */
	float GetRunCountdownDuration() const { return RunCountdownDuration; }

	/** 카운트다운 시작과 초 단위 Duration을 UI, 러너, 트랙 시스템에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunCountdownStartedSignature OnRunCountdownStarted;

	/** 실제 러닝 시작을 러너, 적재물, 이벤트, 점수 시스템에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunStartedSignature OnRunStarted;

	/** 런 실패 원인 확정을 결과 UI와 관련 시스템에 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunFailedSignature OnRunFailed;

	/** 이어하기 카운트다운 시작과 초 단위 Duration을 전달합니다. 러너는 실패 지점에 다시 서고 결과 화면은 닫힙니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunCountdownStartedSignature OnRunContinueCountdownStarted;

	/** 이어하기 카운트다운이 끝나 점수·거리·남은 적재물을 유지한 채 런을 다시 시작할 때 발생합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunStartedSignature OnRunResumed;

private:
	void FinishRunCountdown();
	void HandleRetryCountdownFallback();
	APGRunnerCharacter* EnsureRunnerPossessedForRun();
	APGRunnerCharacter* FindSingleRunnerInWorld() const;
	bool ValidateCurrentMapSetup();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run", meta = (AllowPrivateAccess = "true"))
	EPGRunState RunState = EPGRunState::Home;

	FTimerHandle RunCountdownTimerHandle;
	FTimerHandle RetryCountdownFallbackTimerHandle;
	// StartFlow=RetryCountdown URL 옵션으로 열린 레벨이면 첫 시작 대신 짧은 Retry 카운트다운을 쓴다.
	bool bIsRetryEntry = false;
	float RunCountdownDuration = 0.0f;
	bool bRunRewardsSettled = false;
	bool bIsCurrentMapSetupValid = false;
	// 이번 런에서 이어하기를 쓴 횟수와, 실패 시 이미 지갑에 정산한 런 영수증. 재개 뒤 정산은 그 차액만 더한다.
	int32 ContinueCount = 0;
	int32 SettledRunReceipts = 0;
	// 진행 중인 카운트다운이 이어하기용이면 끝날 때 OnRunStarted 대신 OnRunResumed를 보낸다.
	bool bIsContinueCountdown = false;
};
