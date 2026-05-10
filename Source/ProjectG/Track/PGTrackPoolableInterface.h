// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PGTrackPoolableInterface.generated.h"

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
	// 세그먼트가 풀에서 꺼내져 다시 사용될 때 수집/트리거 상태를 초기화합니다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ProjectG | Track Pool")
	void OnTrackSegmentActivatedFromPool();

	// 세그먼트가 풀로 반환될 때 런타임 상태를 정리합니다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ProjectG | Track Pool")
	void OnTrackSegmentDeactivatedForPool();
};
