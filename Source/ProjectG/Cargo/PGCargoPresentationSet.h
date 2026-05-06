// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PGCargoPresentationSet.generated.h"

class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;
class UUserWidget;

/**
 * 적재물 자동 연출이 반응할 수 있는 로컬 피드백 이벤트 종류입니다.
 * 시작 인트로 컷신은 Level Sequence 책임이므로 여기에는 포함하지 않습니다.
 */
UENUM(BlueprintType)
enum class EPGCargoFeedbackType : uint8
{
	None,
	CargoDropHoldStarted,
	CargoDropHoldFinished,
	CargoDelivered,
	CargoLost
};

/**
 * 적재물 이벤트 하나에 대응하는 UI/VFX/SFX 리소스 묶음입니다.
 * 리소스만 에디터에서 지정하고, 실제 재생과 정리는 CargoFeedbackComponent가 담당합니다.
 */
USTRUCT(BlueprintType)
struct FPGCargoPresentationEntry
{
	GENERATED_BODY()

	// 리소스를 적용할 적재물 피드백 이벤트 종류입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation")
	EPGCargoFeedbackType FeedbackType = EPGCargoFeedbackType::None;

	// 이벤트 발생 시 화면에 표시할 위젯 클래스입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation | UI")
	TSoftClassPtr<UUserWidget> WidgetClass;

	// 위젯/VFX를 자동 정리하기까지의 시간입니다. 0이면 짝이 되는 종료 이벤트까지 유지합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation | UI", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10.0"))
	float DisplayDuration = 1.5f;

	// 이벤트 발생 시 러너 기준으로 재생할 Niagara 시스템입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation | VFX")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	// 이벤트 발생 시 한 번 재생할 사운드입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation | SFX")
	TSoftObjectPtr<USoundBase> Sound;

	// 이벤트가 유지되는 동안 반복 재생할 사운드입니다. 종료/취소 시 자동 정지합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation | SFX")
	TSoftObjectPtr<USoundBase> LoopSound;

	// 이벤트 발생 시 로컬 플레이어 카메라에 적용할 카메라 쉐이크입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation | Camera")
	TSoftClassPtr<UCameraShakeBase> CameraShakeClass;
};

/**
 * 추가 적재/손실 이벤트별 로컬 연출 리소스를 모아두는 DataAsset입니다.
 * 개발자는 이 에셋에 리소스만 할당하면 C++ 피드백 컴포넌트가 자동 실행합니다.
 */
UCLASS(BlueprintType)
class PROJECTG_API UPGCargoPresentationSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 적재물 피드백 이벤트별 로컬 연출 리소스 목록입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo Presentation")
	TArray<FPGCargoPresentationEntry> Entries;
};
