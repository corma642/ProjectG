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
	// 모든 기본 UI 버튼에서 공유하는 클릭음을 2D 사운드로 재생합니다.
	static void PlayCommonButtonClick(const UObject* WorldContextObject);

	// 지정한 사운드를 사용자 SFX 볼륨과 추가 볼륨 배율을 적용해 2D로 재생합니다.
	static void PlaySFX2D(const UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier = 1.0f);

	// 지정한 사운드를 사용자 SFX 볼륨과 추가 볼륨 배율을 적용해 월드 위치에서 재생합니다.
	static void PlaySFXAtLocation(const UObject* WorldContextObject, class USoundBase* Sound, const FVector& Location, float VolumeMultiplier = 1.0f);
};
