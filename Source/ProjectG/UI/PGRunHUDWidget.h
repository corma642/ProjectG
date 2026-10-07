// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGRunHUDWidget.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UPanelWidget;
class UPGCargoDeliveryItemWidget;
class UPGCargoScoreIndicatorWidget;
class UTextBlock;
class UWidget;
struct FPGActiveCargoInfo;

/** Pause 버튼 입력을 HUD 소유 컴포넌트에 전달합니다. */
DECLARE_MULTICAST_DELEGATE(FPGRunHUDPauseRequestedSignature);

/**
 * 러닝 중 점수, 거리, 영수증, 배달 적재물 리스트를 표시하는 HUD 로직 위젯입니다.
 * BP는 모바일 화면 배치와 카드 스타일을 담당하고, C++은 값 갱신, Pause 요청, 카드 풀링만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGRunHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Pause 모달 생성을 상위 HUD 컴포넌트에 요청합니다. */
	FPGRunHUDPauseRequestedSignature OnPauseRequested;

	/** 적재물 카드 풀을 생성할 Widget Blueprint 클래스를 설정합니다. */
	void SetCargoItemWidgetClass(TSubclassOf<UPGCargoDeliveryItemWidget> InCargoItemWidgetClass);

	/** 배달 완료 점수 인디케이터를 생성할 Widget Blueprint 클래스를 설정합니다. */
	void SetCargoScoreIndicatorWidgetClass(TSubclassOf<UPGCargoScoreIndicatorWidget> InCargoScoreIndicatorWidgetClass);

	/** 현재 런 점수를 정수 텍스트로 갱신합니다. */
	void SetScore(int32 Score);

	/** 표시 거리를 미터 단위 텍스트로 갱신합니다. */
	void SetDistanceMeters(float DistanceMeters);

	/** 이번 런에서 획득한 영수증 수를 갱신합니다. */
	void SetReceipts(int32 Receipts);

	/** 전환 또는 Pause 중 HUD의 Pause 입력을 잠급니다. */
	void SetPauseButtonEnabled(bool bEnabled) const;

	/** 활성 적재물 스냅샷과 카드 풀을 ID 기준으로 동기화합니다. */
	void RefreshCargoList(const TArray<FPGActiveCargoInfo>& ActiveCargoInfos);

	/** 지정 적재물 카드 위치에 배달 보너스 점수 인디케이터를 표시합니다. */
	void ShowCargoScoreIndicator(int32 CargoInstanceId, int32 Score);

	/** 적재물 손실 임박 경고 오버레이를 켜거나 끕니다. 켜진 동안 NativeTick에서 펄스로 깜빡입니다. */
	void SetCargoDangerWarningActive(bool bActive);

	/** 런 HUD 진입 애니메이션을 시작합니다. */
	void PlayEnterAnimation();

	/** HUD 컴포넌트가 정리될 때(레벨 종료·Home 전환) 퇴장 애니메이션을 재생한 뒤 스스로 제거합니다. */
	void PlayExitAnimation();

protected:
	/** Pause 버튼 delegate를 연결합니다. */
	virtual void NativeConstruct() override;

	/** Pause 버튼 delegate를 위젯 제거 전에 해제합니다. 카드 delegate는 약한 참조라 함께 소멸합니다. */
	virtual void NativeDestruct() override;

	/** HUD 진입/퇴장 애니메이션과 손실 임박 경고 펄스가 켜진 동안에만 표현 상태를 갱신합니다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// HUD 전체 루트 패널입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UWidget> RootPanel;

	// 현재 런 점수 표시 텍스트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ScoreText;

	// 5m 단위로 갱신되는 진행 거리 표시 텍스트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> DistanceText;

	// 현재 런에서 획득한 영수증 표시 텍스트입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UTextBlock> ReceiptsText;

	/** 러닝 중 Pause 모달 생성을 요청하는 필수 버튼입니다. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UButton> PauseButton;

	// 하단 컨베이어 형태의 활성 배달 적재물 카드 목록입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UPanelWidget> CargoListBox;

	// 배달 완료 점수 인디케이터를 카드 위치 위에 띄우기 위한 Canvas 레이어입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "UI")
	TObjectPtr<UCanvasPanel> CargoScoreIndicatorLayer;

	// 적재물 손실 임박 시 깜빡이는 경고 오버레이입니다. BP에 없으면 경고음만 재생됩니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional), Category = "UI")
	TObjectPtr<UImage> CargoDangerImage;

private:
	UFUNCTION()
	void HandlePauseClicked();

	void ApplyAnimationState(float Alpha, bool bExiting);
	void UpdateCargoDangerPulse(float InDeltaTime);
	UPGCargoDeliveryItemWidget* AcquireCargoItemWidget();
	FVector2D ResolveCargoIndicatorPosition(int32 CargoInstanceId) const;
	void HandleCargoItemExitFinished(UPGCargoDeliveryItemWidget* ItemWidget);

	UPROPERTY(Transient)
	TSubclassOf<UPGCargoDeliveryItemWidget> CargoItemWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UPGCargoScoreIndicatorWidget> CargoScoreIndicatorWidgetClass;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UPGCargoDeliveryItemWidget>> ActiveItemWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPGCargoDeliveryItemWidget>> InactiveItemPool;

	bool bIsEntering = false;
	bool bIsExiting = false;
	bool bIsCargoDangerActive = false;
	float AnimationElapsed = 0.0f;
	float AnimationDuration = 0.22f;
	float CargoDangerPulseElapsed = 0.0f;
};
