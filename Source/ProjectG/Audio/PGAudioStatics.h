// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PGAudioStatics.generated.h"

/**
 * 프로젝트 공통 UI/SFX 재생 헬퍼입니다.
 * 사운드 설정은 Project Settings가 담당하고, 이 클래스는 사용자 SFX 볼륨을 적용해 재생만 수행합니다.
 */
UCLASS()
class PROJECTG_API UPGAudioStatics : public UObject
{
	GENERATED_BODY()

public:
	/** 모든 기본 UI 버튼에서 공유하는 클릭음을 사용자 SFX 배율로 2D 재생합니다. 유효 월드가 없으면 재생하지 않습니다. */
	static void PlayCommonButtonClick(const UObject* WorldContextObject);

	/** 전용 사운드가 있으면 재생하고 없으면 공통 클릭음으로 fallback합니다. VolumeMultiplier는 추가 선형 배율입니다. */
	static void PlayUIButtonSound(const UObject* WorldContextObject, class USoundBase* SpecificSound = nullptr, float VolumeMultiplier = 1.0f);

	/** 지정 사운드를 사용자 SFX 배율과 추가 선형 배율을 적용해 2D 재생합니다. null이면 아무 동작도 하지 않습니다. */
	static void PlaySFX2D(const UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier = 1.0f);

	/** 지정 사운드를 사용자 SFX 배율과 추가 선형 배율로 월드 위치(cm)에서 재생합니다. null이면 재생하지 않습니다. */
	static void PlaySFXAtLocation(const UObject* WorldContextObject, class USoundBase* Sound, const FVector& Location, float VolumeMultiplier = 1.0f);
};
