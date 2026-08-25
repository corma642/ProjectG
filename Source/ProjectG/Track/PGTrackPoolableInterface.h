// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PGTrackPoolableInterface.generated.h"

/** Blueprint Actor가 트랙 세그먼트 풀링 생명주기에 참여하도록 노출하는 인터페이스 타입입니다. */
UINTERFACE(BlueprintType)
class PROJECTG_API UPGTrackPoolableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 트랙 세그먼트 내부에 배치되어 세그먼트 풀링 생명주기에 맞춰 상태를 리셋해야 하는 Actor용 인터페이스입니다.
 */
class PROJECTG_API IPGTrackPoolableInterface
{
	GENERATED_BODY()

public:
	/** 세그먼트가 풀에서 꺼내진 직후 수집·트리거 상태를 새 배치 기준으로 초기화합니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ProjectG | Track Pool")
	void OnTrackSegmentActivatedFromPool();

	/** 세그먼트가 비활성화되기 직전 delegate와 일회성 런타임 상태를 정리합니다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ProjectG | Track Pool")
	void OnTrackSegmentDeactivatedForPool();
};
