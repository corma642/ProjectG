// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PGQuitConfirmWidget.generated.h"

class UBackgroundBlur;
class UButton;

DECLARE_MULTICAST_DELEGATE(FPGQuitConfirmedSignature);
DECLARE_MULTICAST_DELEGATE(FPGQuitCancelledSignature);

/**
 * Home에서 Android 뒤로가기를 눌렀을 때 앱 종료를 묻는 모달 Blueprint Widget의 로직 클래스입니다.
 * BP는 패널·버튼 스타일을 담당하고 C++은 버튼 이벤트와 배경 블러 세기만 처리합니다.
 */
UCLASS()
class PROJECTG_API UPGQuitConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 종료 버튼을 눌러 앱을 끝내기로 했을 때 상위 흐름(HomeFlow)에 알립니다. */
	FPGQuitConfirmedSignature OnQuitConfirmed;

	/** 취소 버튼이나 뒤로가기로 모달을 닫을 때 알립니다. */
	FPGQuitCancelledSignature OnQuitCancelled;

protected:
	/** 버튼 delegate를 연결하고 배경 블러에 고정 세기를 적용합니다. */
	virtual void NativeConstruct() override;

	/** 버튼 delegate를 제거 전에 해제합니다. */
	virtual void NativeDestruct() override;

	// 앱 종료를 확정하는 필수 버튼입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Quit")
	TObjectPtr<UButton> ConfirmButton;

	// 모달을 닫는 필수 버튼입니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget), Category = "Quit")
	TObjectPtr<UButton> CancelButton;

	// 뒤 Home 화면을 흐리게 하는 선택 블러입니다. 있으면 C++이 Pause와 같은 세기를 적용합니다.
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional), Category = "Quit")
	TObjectPtr<UBackgroundBlur> BackdropBlur;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
