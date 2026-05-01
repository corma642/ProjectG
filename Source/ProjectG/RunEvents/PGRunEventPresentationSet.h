// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RunEvents/PGRunEventSet.h"
#include "PGRunEventPresentationSet.generated.h"

class UNiagaraSystem;
class USoundBase;
class UUserWidget;

/**
 * 런 이벤트 하나에 대응하는 로컬 연출 리소스 묶음입니다.
 * UI, VFX, SFX 에셋을 데이터로만 지정하고 실제 실행은 FeedbackComponent가 담당합니다.
 */
USTRUCT(BlueprintType)
struct FPGRunEventPresentationEntry
{
	GENERATED_BODY()

	// 리소스를 적용할 이벤트 종류입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation")
	EPGRunEventType EventType = EPGRunEventType::None;

	// 이벤트 전조 중 화면에 표시할 위젯 클래스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | UI")
	TSoftClassPtr<UUserWidget> PreviewWidgetClass;

	// 이벤트 적용 중 화면에 표시할 위젯 클래스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | UI")
	TSoftClassPtr<UUserWidget> ActiveWidgetClass;

	// 이벤트 전조 중 러너 주변에 표시할 Niagara 시스템입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | VFX")
	TSoftObjectPtr<UNiagaraSystem> PreviewNiagaraSystem;

	// 이벤트 적용 중 러너 주변에 표시할 Niagara 시스템입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | VFX")
	TSoftObjectPtr<UNiagaraSystem> ActiveNiagaraSystem;

	// 이벤트 전조 시작 시 재생할 단발 사운드입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | SFX")
	TSoftObjectPtr<USoundBase> PreviewSound;

	// 이벤트 발동 순간 재생할 단발 사운드입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | SFX")
	TSoftObjectPtr<USoundBase> TriggerSound;

	// 이벤트 적용 중 반복 재생할 사운드입니다. 종료/취소 시 자동 정지됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation | SFX")
	TSoftObjectPtr<USoundBase> ActiveLoopSound;
};

/**
 * 이벤트별 UI/VFX/SFX 리소스를 모아두는 DataAsset입니다.
 * 사용자는 이 자산에 리소스만 할당하면 런 이벤트 연출이 자동 실행됩니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGRunEventPresentationSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 이벤트 타입별 로컬 연출 리소스 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Event Presentation")
	TArray<FPGRunEventPresentationEntry> Entries;
};
