// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "PGTrackEventMarkerComponent.generated.h"

/**
 * 자동 생성 세그먼트에 배치되는 이벤트 종류입니다.
 * 실제 효과 적용은 플레이어가 마커를 통과할 때 RunnerEnvironmentComponent로 전달됩니다.
 */
UENUM(BlueprintType)
enum class EPGTrackEventType : uint8
{
	None UMETA(DisplayName = "None"),
	Wind UMETA(DisplayName = "Wind"),
	Ice UMETA(DisplayName = "Ice"),
	CargoDrop UMETA(DisplayName = "Cargo Drop")
};

/**
 * 자동 이벤트 마커의 런타임 상태입니다.
 * BP는 이 값을 읽어 표지판, 바람 방향 UI, 얼음 노면 표시, 추가 계란 예고 연출을 붙일 수 있습니다.
 */
UENUM(BlueprintType)
enum class EPGTrackEventState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Preview UMETA(DisplayName = "Preview"),
	Armed UMETA(DisplayName = "Armed"),
	Triggered UMETA(DisplayName = "Triggered")
};

/**
 * 트랙 세그먼트 안에서 환경 이벤트나 추가 적재물을 발동시키는 오버랩 마커입니다.
 * 디자이너는 위치와 크기만 배치하고, 런타임 이벤트 종류는 RunDirector가 난이도에 맞춰 주입합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGTrackEventMarkerComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UPGTrackEventMarkerComponent();

	// RunDirector가 세그먼트 생성 시 이번 마커에 적용할 이벤트를 지정합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track Event")
	void ConfigureGeneratedEvent(EPGTrackEventType InEventType, float DifficultyAlpha, int32 EventSeed);

	// 생성된 이벤트를 비우고, 이 마커가 다시 발동하지 않도록 초기화합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track Event")
	void ClearGeneratedEvent();

	// 예고 상태의 이벤트를 발동 가능 상태로 전환합니다. 별도 연출이 없으면 Preview 상태에서도 오버랩 시 자동 발동됩니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track Event")
	void ArmGeneratedEvent();

	// 자동 이벤트 배정 대상인지 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track Event")
	bool CanReceiveGeneratedEvent() const { return bAllowGeneratedEvents; }

	// 현재 이 마커에 배정된 이벤트 종류를 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track Event")
	EPGTrackEventType GetGeneratedEventType() const { return GeneratedEventType; }

	// 현재 이벤트 마커 상태를 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track Event")
	EPGTrackEventState GetGeneratedEventState() const { return GeneratedEventState; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleMarkerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 플레이어 캐릭터에 붙은 환경 컴포넌트로 실제 이벤트 효과를 전달합니다.
	void ApplyGeneratedEvent(AActor* RunnerActor);

	// RunDirector가 이 마커에 이벤트를 자동 배정할 수 있는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Event", meta = (AllowPrivateAccess = "true"))
	bool bAllowGeneratedEvents = true;

	// 바람 이벤트의 기본 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Event | Wind", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0"))
	float WindDuration = 3.0f;

	// 바람 이벤트의 기본 강도. 난이도가 오르면 이 값 안에서 조금 더 강하게 적용됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Event | Wind", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float WindStrength = 1.0f;

	// 얼음 길 이벤트의 기본 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Event | Ice", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0"))
	float IceDuration = 3.0f;

	// 얼음 길에서 입력이 목표값을 따라가는 비율. 낮을수록 미끄럽고 반응이 느립니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Event | Ice", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.05", UIMax = "1.0"))
	float IceInputLagScale = 0.35f;

	// RunDirector가 이번 런타임에 배정한 이벤트 종류
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Track Event", meta = (AllowPrivateAccess = "true"))
	EPGTrackEventType GeneratedEventType = EPGTrackEventType::None;

	// RunDirector가 배정한 이벤트의 현재 예고/발동 상태
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Track Event", meta = (AllowPrivateAccess = "true"))
	EPGTrackEventState GeneratedEventState = EPGTrackEventState::Inactive;

	// 이번 이벤트에서 사용할 바람 방향
	float GeneratedWindDirection = 1.0f;

	// 이번 이벤트에서 사용할 바람 강도
	float GeneratedWindStrength = 1.0f;

	// 같은 마커가 한 번의 런에서 반복 발동하지 않도록 막는 상태
	bool bHasTriggered = false;
};
