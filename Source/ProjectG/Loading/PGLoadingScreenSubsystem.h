// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGLoadingScreenSubsystem.generated.h"

class SWidget;
class UTexture2D;
struct FPGLoadingScreenSettings;

/**
 * 레벨 전환(Retry, Home 복귀, 맵 변경) 동안 엔진 MoviePlayer로 로딩 화면(배경색·로고·회전 표시)을 띄우는 GameInstance Subsystem입니다.
 * PreLoadMap마다 Slate 위젯을 만들어 등록하고 재생을 시작하면, 게임 스레드가 로드에 막혀 있는 동안 로딩 스레드가 화면을 그리고
 * 로드가 끝나면 MoviePlayer가 스스로 닫습니다. 에디터(PIE)에서는 MoviePlayer가 꺼져 있어 아무 일도 하지 않습니다.
 */
UCLASS()
class PROJECTG_API UPGLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** MoviePlayer를 쓸 수 있는 실행(게임 빌드, 멀티스레드 렌더링)에서만 맵 로드 delegate를 연결합니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 맵 로드 delegate를 해제합니다. */
	virtual void Deinitialize() override;

	/**
	 * 설정대로 로딩 화면 Slate 위젯을 만듭니다. LogoBrush가 null이면 로고 없이 배경과 회전 표시만 그립니다.
	 * DPIScale은 1080px 폭 기준 디자인 배율입니다(UMG와 같은 짧은 변 규칙). 에디터 위젯 QA 캡처도 이 함수로 그려 게임과 같은 모양을 확인합니다.
	 */
	static TSharedRef<SWidget> MakeLoadingScreenWidget(const FPGLoadingScreenSettings& Settings, const FSlateBrush* LogoBrush, float DPIScale);

	/** 로고 텍스처를 설정의 로고 폭에 맞는 Slate 브러시로 채웁니다. 텍스처가 없으면 false를 돌려주고 브러시를 비웁니다. */
	static bool MakeLogoBrush(const FPGLoadingScreenSettings& Settings, UTexture2D* InLogoTexture, FSlateBrush& OutBrush);

private:
	void HandlePreLoadMap(const FString& MapName);

	// MoviePlayer가 로딩 스레드에서 그리는 동안 로고 텍스처가 GC되지 않게 잡아 둔다.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LogoTexture;

	FSlateBrush LogoBrush;
	FDelegateHandle PreLoadMapHandle;
};
