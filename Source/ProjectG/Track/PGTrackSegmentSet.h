// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGTrackSegmentSet.generated.h"

class APGTrackSegment;

/**
 * 자동 생성 후보 트랙 세그먼트 하나의 선택 규칙입니다.
 * 난이도, 가중치, 쿨다운을 함께 써서 초반 안전 조합과 반복 등장을 제어합니다.
 */
USTRUCT(BlueprintType)
struct FPGTrackSegmentEntry
{
	GENERATED_BODY()

	// 생성할 트랙 세그먼트 Blueprint 클래스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment")
	TSubclassOf<APGTrackSegment> SegmentClass;

	// 기본 선택 가중치입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float BaseWeight = 1.0f;

	// 난이도가 1.0에 가까워질수록 추가되는 가중치입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float DifficultyWeight = 0.0f;

	// 이 난이도 이상부터 선택 후보가 됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinDifficultyAlpha = 0.0f;

	// 같은 세그먼트가 다시 등장하기 전 필요한 최소 세그먼트 수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment", meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 CooldownSegments = 0;

	// 같은 세그먼트가 연속으로 등장할 수 있는 최대 횟수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment", meta = (ClampMin = "1", UIMin = "1", UIMax = "5"))
	int32 MaxConsecutiveCount = 2;

	// 선택 규칙이 모두 막혔을 때 길이 끊기지 않도록 사용할 안전 후보인지 여부입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment")
	bool bCanBeFallback = true;
};

/**
 * 무한 러닝에서 사용할 트랙 세그먼트 후보 목록 DataAsset입니다.
 * 환경 이벤트는 RunEventSubsystem이 별도로 관리하므로 이 데이터는 길 선택만 담당합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGTrackSegmentSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 무한 러닝에서 선택 가능한 세그먼트 후보 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment Set")
	TArray<FPGTrackSegmentEntry> SegmentEntries;

	// 모든 후보가 막혔을 때 우선 사용할 안전 세그먼트 클래스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track Segment Set")
	TSubclassOf<APGTrackSegment> FallbackSegmentClass;
};
