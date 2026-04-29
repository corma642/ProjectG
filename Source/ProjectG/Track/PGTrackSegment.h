// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGTrackSegment.generated.h"

class USceneComponent;

/**
 * 무한 러닝 길을 구성하는 직선형 세그먼트 기본 Actor입니다.
 * C++은 시작점, 끝점, 길이, 이벤트 허용 여부만 관리하고 메시/콜리전/장식은 Blueprint가 구성합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGTrackSegment : public AActor
{
	GENERATED_BODY()

public:
	APGTrackSegment();

	virtual void OnConstruction(const FTransform& Transform) override;

	// 세그먼트가 이어 붙는 시작 월드 위치입니다. 기본적으로 Actor 위치와 같습니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FVector GetSegmentStartWorldLocation() const;

	// 다음 세그먼트의 시작점으로 사용할 끝 월드 위치입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FVector GetSegmentEndWorldLocation() const;

	// 직선형 세그먼트의 진행 방향입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FVector GetSegmentForwardVector() const;

	// 세그먼트 길이를 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	float GetSegmentLength() const { return SegmentLength; }

	// 이 세그먼트가 자동 이벤트 배정을 허용하는지 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	bool AllowsGeneratedEvents() const { return bAllowGeneratedEvents; }

	// 세그먼트 구분용 태그입니다. 디버그와 향후 선택 규칙 확장에 사용합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FName GetSegmentTag() const { return SegmentTag; }

	// 풀에서 꺼낸 세그먼트를 새 Transform으로 재활성화합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track")
	virtual void ActivateFromPool(const FTransform& SegmentTransform);

	// 지나간 세그먼트를 풀에 반환하기 전 비활성화합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track")
	virtual void DeactivateForPool();

	// 세그먼트에 포함된 자동 이벤트 마커 상태를 초기화합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track")
	void ResetGeneratedEventMarkers();

protected:
	// 세그먼트의 루트이자 시작 기준점입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<USceneComponent> SegmentRoot;

	// 에디터에서 시작 기준을 확인하기 위한 컴포넌트입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<USceneComponent> StartPoint;

	// 에디터에서 끝 기준을 확인하기 위한 컴포넌트입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<USceneComponent> EndPoint;

	// 직선형 세그먼트 길이. Actor X축 방향으로 다음 세그먼트가 이어집니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "100.0", UIMin = "500.0", UIMax = "5000.0"))
	float SegmentLength = 2000.0f;

	// 자동 이벤트 마커 배정을 허용할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track")
	bool bAllowGeneratedEvents = true;

	// 디버그와 선택 규칙 확장을 위한 세그먼트 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track")
	FName SegmentTag = TEXT("Default");
};
