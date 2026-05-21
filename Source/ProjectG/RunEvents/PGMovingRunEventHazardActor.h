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
	APGMovingRunEventHazardActor();

	// RunEventSubsystem이 발동 시점에 공통 설정과 선택된 가상 구역 정보를 주입합니다.
	void InitializeMovingHazard(const FVector& InMovementVelocity, float InLifetime, float InPushImpulseVelocity, float InMaxPushBalanceVelocity, float InRehitCooldown, float InPushDirectionSign);

	UBoxComponent* GetOverlapBox() const { return OverlapBox; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleHazardOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	float ResolvePushDirectionSign(const AActor& RunnerActor) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moving Hazard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> HazardRoot;

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
