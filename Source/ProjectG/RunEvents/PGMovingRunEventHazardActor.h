// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGMovingRunEventHazardActor.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * RunEvent가 스폰하는 이동형 위험 구역 베이스 Actor입니다.
 *
 * C++은 이동, 수명, 오버랩 시 Balance 밀림만 담당합니다. 까마귀 무리 메시, Niagara, 사운드 같은
 * 콘텐츠 표현은 이 클래스를 상속한 Blueprint에서 구성합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGMovingRunEventHazardActor : public AActor
{
	GENERATED_BODY()

public:
	/** 이동 Tick과 러너 overlap 전용 BoxComponent를 구성합니다. */
	APGMovingRunEventHazardActor();

	/** 발동 시 월드 속도(cm/s), 수명·재충돌 간격(초), Balance 충격 속도(cm/s)와 -1~1 방향을 주입합니다. */
	void InitializeMovingHazard(const FVector& InMovementVelocity, float InLifetime, float InPushImpulseVelocity, float InMaxPushBalanceVelocity, float InRehitCooldown, float InPushDirectionSign);

	/** Blueprint가 위험 구역 크기와 collision 표현을 조정할 BoxComponent를 반환합니다. */
	UBoxComponent* GetOverlapBox() const { return OverlapBox; }

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

	/** Blueprint 표현과 이동의 기준이 되는 필수 루트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> HazardRoot;

	/** 러너에게 Balance 충격을 주는 overlap 영역이며 크기와 profile은 BP에서 조정합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> OverlapBox;

	FVector MovementVelocity = FVector(-800.0f, 0.0f, 0.0f);
	float RemainingLifetime = 5.0f;
	float PushImpulseVelocity = 850.0f;
	float MaxPushBalanceVelocity = 1000.0f;
	float RehitCooldown = 0.75f;
	float PushDirectionSign = 0.0f;
	float LastHitWorldTime = -FLT_MAX;
};
