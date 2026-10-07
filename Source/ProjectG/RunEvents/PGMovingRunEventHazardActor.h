// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Settings/PGGameSettings.h"
#include "PGMovingRunEventHazardActor.generated.h"

class APGRunnerCharacter;
class UBoxComponent;
class USceneComponent;

/**
 * RunEvent가 스폰하는 이동형 위험 구역 베이스 Actor입니다.
 *
 * C++은 이동, 수명, 오버랩 시 Balance 밀림과 피격 피드백(효과음·카메라 쉐이크·적재물 충격)만 담당합니다.
 * 까마귀 무리 메시, Niagara, 비행 사운드 같은 콘텐츠 표현은 이 클래스를 상속한 Blueprint에서 구성합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGMovingRunEventHazardActor : public AActor
{
	GENERATED_BODY()

public:
	/** 이동 Tick과 러너 overlap 전용 BoxComponent를 구성합니다. */
	APGMovingRunEventHazardActor();

	/** 발동 시 Project Settings 복사본, 월드 속도(cm/s), 수명(초), 이벤트 규칙 Strength 배율과 -1~1 밀기 방향을 주입합니다. */
	void InitializeMovingHazard(const FPGMovingHazardSettings& InSettings, const FVector& InMovementVelocity, float InLifetime, float InStrength, float InPushDirectionSign);

protected:
	/** 러너 overlap delegate를 연결합니다. */
	virtual void BeginPlay() override;
	/** 월드 종료 또는 수명 만료 전에 overlap delegate를 해제합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** 설정된 월드 속도로 이동하고 초 단위 수명이 끝나면 Actor를 제거합니다. */
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleHazardOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	float ResolvePushDirectionSign(const AActor& RunnerActor) const;
	void PlayHitFeedback(APGRunnerCharacter& RunnerCharacter, float DirectionSign) const;

	/** Blueprint 표현과 이동의 기준이 되는 필수 루트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> HazardRoot;

	/** 러너에게 Balance 충격을 주는 overlap 영역이며 크기와 profile은 BP에서 조정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> OverlapBox;

	// 발동 시점의 Project Settings 복사본입니다. 밀기 세기, 재충돌 간격, 피격 피드백 리소스를 읽습니다.
	FPGMovingHazardSettings CachedSettings;
	FVector MovementVelocity = FVector(-800.0f, 0.0f, 0.0f);
	float RemainingLifetime = 5.0f;
	// 이벤트 규칙의 Strength 배율입니다. Balance 밀기와 적재물 충격 속도에 모두 곱합니다.
	float Strength = 1.0f;
	float PushDirectionSign = 0.0f;
	float LastHitWorldTime = -FLT_MAX;
};
