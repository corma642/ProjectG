// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGTrackSegment.generated.h"

class USceneComponent;

/**
 * 무한 러닝 길을 구성하는 직선형 트랙 세그먼트 기본 Actor입니다.
 * C++은 시작점, 끝점, 길이만 관리하고 메시/콜리전/장식은 Blueprint에서 구성합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGTrackSegment : public AActor
{
	GENERATED_BODY()

public:
	/** 시작·끝 기준 컴포넌트와 풀링 가능한 기본 Actor 상태를 구성합니다. */
	APGTrackSegment();

	/** 에디터에서 길이가 바뀔 때 EndPoint를 Actor 로컬 X축의 SegmentLength 위치로 맞춥니다. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 세그먼트가 이어 붙는 StartPoint의 월드 위치를 cm 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FVector GetSegmentStartWorldLocation() const;

	/** 다음 세그먼트 시작점으로 사용할 EndPoint의 월드 위치를 cm 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FVector GetSegmentEndWorldLocation() const;

	/** 세그먼트 Actor의 월드 전방 단위 벡터를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FVector GetSegmentForwardVector() const;

	/** 직선형 세그먼트 길이를 cm 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	float GetSegmentLength() const { return SegmentLength; }

	/** 디버그와 선택 규칙 확장에 사용하는 세그먼트 태그를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Track")
	FName GetSegmentTag() const { return SegmentTag; }

	/** 풀에서 꺼낸 세그먼트를 지정 월드 Transform으로 재활성화하고 내부 풀링 대상에 알립니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track")
	virtual void ActivateFromPool(const FTransform& SegmentTransform);

	/** 지나간 세그먼트를 숨기고 collision/tick을 끈 뒤 내부 풀링 대상에 알립니다. */
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Track")
	virtual void DeactivateForPool();

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

	// 직선형 세그먼트 길이입니다. Actor X축 방향으로 다음 세그먼트가 이어집니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "100.0", UIMin = "500.0", UIMax = "5000.0"))
	float SegmentLength = 2000.0f;

	// 디버그와 선택 규칙 확장을 위한 세그먼트 태그입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track")
	FName SegmentTag = TEXT("Default");
};
