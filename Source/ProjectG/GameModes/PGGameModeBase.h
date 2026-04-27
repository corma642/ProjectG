// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PGGameModeBase.generated.h"

/**
 * 현재 한 판 플레이가 어떤 상태인지 나타내는 런 상태
 */
UENUM(BlueprintType)
enum class EPGRunState : uint8
{
	WaitingToStart,
	Running,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRunFailedSignature, FName, FailureReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGCargoPenaltySignature, int32, LostSlotIndex);

/**
 * Go Deliver!의 기본 Pawn/Controller 연결과 한 판 실패 상태를 관리하는 게임모드
 */
UCLASS()
class PROJECTG_API APGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	APGGameModeBase();

	// 게임 시작 시 한 판 플레이 상태를 Running으로 전환한다.
	virtual void BeginPlay() override;

	// 핵심 적재물 손실, 추락 등으로 한 판 실패를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	void RequestRunFailed(FName FailureReason);

	// 보너스 적재물 손실처럼 게임오버가 아닌 페널티 흐름을 요청한다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Run")
	void RequestCargoPenalty(int32 LostSlotIndex);

	// 현재 한 판 플레이 상태
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run")
	EPGRunState GetRunState() const { return RunState; }

	// 한 판 실패가 확정될 때 블루프린트/UI로 전달하는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGRunFailedSignature OnRunFailed;

	// 보너스 적재물 손실 페널티를 블루프린트/UI로 전달하는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Run")
	FPGCargoPenaltySignature OnCargoPenalty;

private:
	// 현재 한 판 플레이 상태
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run", meta = (AllowPrivateAccess = "true"))
	EPGRunState RunState = EPGRunState::WaitingToStart;
};
