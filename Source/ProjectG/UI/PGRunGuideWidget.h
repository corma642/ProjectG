// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunGuideWidget.generated.h"

class UWidget;

/** 첫 런 가이드가 현재 안내하는 조작 단계입니다. */
UENUM(BlueprintType)
enum class EPGRunGuideStep : uint8
{
	None,
	Drag,
	Flick
};

/**
 * 첫 런에서 드래그(균형)와 위쪽 플릭(점프)을 한 단계씩 안내하는 HUD 오버레이 위젯입니다.
 * BP는 문구, 아이콘, 배치, 스타일을 담당하고 C++은 단계 전환, 제스처 아이콘의 반복 이동, 페이드만 처리합니다.
 * 입력은 전혀 받지 않으며 단계 완료 판정은 UPGRunHUDComponent가 러너 입력으로 내립니다.
 */
UCLASS()
class PROJECTG_API UPGRunGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 지정 단계의 안내만 보이게 하고 페이드 인합니다. None이면 안내를 모두 숨깁니다. */
	UFUNCTION(BlueprintCallable, Category = "Run Guide")
	void ShowStep(EPGRunGuideStep Step);

	/** 페이드 아웃이 끝나면 스스로 viewport에서 제거합니다. */
	void Dismiss();

protected:
	/** 입력을 받지 않는 투명 상태로 시작하고 현재 단계의 안내만 보이게 둡니다. */
	virtual void NativeConstruct() override;

	/** 페이드와 제스처 아이콘 이동을 갱신하고, 페이드 아웃이 끝나면 제거합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 페이드 opacity를 적용하는 필수 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> RootPanel;

	// 드래그(균형) 안내 묶음. Drag 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> DragPrompt;

	// 위쪽 플릭(점프) 안내 묶음. Flick 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> FlickPrompt;

	// Drag 단계에서 좌우로 왕복하는 손가락 아이콘입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> DragGestureIcon;

	// Flick 단계에서 위로 튀어 오른 뒤 사라지기를 반복하는 손가락 아이콘입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> FlickGestureIcon;

private:
	void ApplyStepVisibility() const;

	EPGRunGuideStep CurrentStep = EPGRunGuideStep::None;
	bool bIsDismissing = false;

	// 0은 투명, 1은 완전 표시. ShowStep 뒤 올라가고 Dismiss 뒤 내려간다.
	float FadeAlpha = 0.0f;

	// 제스처 아이콘 반복 이동의 위상 계산용 누적 시간
	float GestureElapsed = 0.0f;
};
