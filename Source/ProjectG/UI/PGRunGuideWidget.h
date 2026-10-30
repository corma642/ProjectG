// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tutorial/PGTutorialTypes.h"
#include "PGRunGuideWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * 튜토리얼 스테이지에서 단계별 조작 안내(균형·점프·좌우 이동·적재물·바람)와 완료 보상 문구를 띄우는 HUD 오버레이 위젯입니다.
 * BP는 문구, 아이콘, 배치, 스타일을 담당하고 C++은 단계 전환, 제스처 아이콘의 반복 이동, 페이드만 처리합니다.
 * 입력은 전혀 받지 않습니다. 단계 완료 판정은 UPGTutorialSubsystem이, 표시 요청은 UPGRunHUDComponent가 맡습니다.
 */
UCLASS()
class PROJECTG_API UPGRunGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 지정 단계의 안내만 보이게 하고 페이드 인합니다. None이면 안내를 페이드 아웃하되 위젯은 다음 단계를 위해 남깁니다. */
	UFUNCTION(BlueprintCallable, Category = "Run Guide")
	void ShowStep(EPGTutorialStep Step);

	/** 완료 문구와 보상 영수증 수를 띄우고 잠시 뒤 스스로 사라집니다. 보상이 0이면(이미 완료했던 플레이) 보상 줄을 숨깁니다. */
	UFUNCTION(BlueprintCallable, Category = "Run Guide")
	void ShowCompletion(int32 RewardReceipts);

	/** 페이드 아웃이 끝나면 스스로 viewport에서 제거합니다. */
	void Dismiss();

	/** Dismiss 뒤 사라지는 중이면 true입니다. 곧 제거되므로 새 안내는 새 위젯에 띄워야 합니다. */
	bool IsDismissing() const { return bIsDismissing; }

protected:
	/** 입력을 받지 않는 투명 상태로 시작하고 현재 단계의 안내만 보이게 둡니다. */
	virtual void NativeConstruct() override;

	/** 페이드와 제스처 아이콘 이동을 갱신하고, 완료 문구 시간이 끝나거나 페이드 아웃이 끝나면 처리합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 페이드 opacity를 적용하는 필수 루트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> RootPanel;

	// 드래그(균형) 안내 묶음. Balance 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> DragPrompt;

	// 위쪽 플릭(점프) 안내 묶음. Jump 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> FlickPrompt;

	// 좌우 플릭(옆 길 이동) 안내 묶음. Lateral 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> LateralPrompt;

	// 적재물 받기 안내 묶음. Cargo 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> CargoPrompt;

	// 바람 버티기 안내 묶음. Wind 단계에서만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> WindPrompt;

	// 완료 문구 묶음. ShowCompletion 동안만 보입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> CompletePrompt;

	// Balance 단계에서 좌우로 왕복하는 손가락 아이콘입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> DragGestureIcon;

	// Jump 단계에서 위로 튀어 오른 뒤 사라지기를 반복하는 손가락 아이콘입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> FlickGestureIcon;

	// Lateral 단계에서 오른쪽·왼쪽으로 번갈아 튕긴 뒤 사라지는 손가락 아이콘입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UWidget> LateralGestureIcon;

	// 완료 문구 아래 보상 줄("영수증 +20"). C++이 지급액으로 채우고 보상이 없으면 숨깁니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Run Guide")
	TObjectPtr<UTextBlock> CompleteRewardText;

private:
	void ApplyStepVisibility() const;
	void UpdateGestureIcons(float DeltaSeconds);

	EPGTutorialStep CurrentStep = EPGTutorialStep::None;
	bool bShowingCompletion = false;
	bool bIsDismissing = false;

	// 0은 투명, 1은 완전 표시. 안내가 있으면 올라가고 없으면(단계 사이·Dismiss) 내려간다.
	float FadeAlpha = 0.0f;

	// 제스처 아이콘 반복 이동의 위상 계산용 누적 시간
	float GestureElapsed = 0.0f;

	// 완료 문구가 떠 있을 남은 시간(초). 0이 되면 스스로 Dismiss한다.
	float CompletionRemaining = 0.0f;
};
