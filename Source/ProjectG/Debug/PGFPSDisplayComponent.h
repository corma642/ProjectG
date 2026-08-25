// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGFPSDisplayComponent.generated.h"

class APGPlayerController;
class UPGFPSDisplayWidget;
class UPGRuntimePreloadSubsystem;
enum class EPGRuntimePreloadGroup : uint8;

/**
 * 로컬 플레이어의 FPS 표시 위젯을 Settings 저장값에 맞춰 생성/제거하는 독립 컴포넌트입니다.
 * 게임 상태와 무관하게 Android 프레임 테스트용 오버레이만 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGFPSDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 설정 delegate로만 구동되도록 ActorComponent Tick을 비활성화합니다. */
	UPGFPSDisplayComponent();

	/** 로컬 PlayerController와 FPS 표시 설정을 연결하고 준비된 UI 리소스로 현재 표시 상태를 복구합니다. */
	virtual void BeginPlay() override;
	/** 설정·preload delegate를 해제하고 생성한 FPS 위젯을 제거합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRuntimePreloadCompleted(EPGRuntimePreloadGroup Group);

	void HandleShowFPSDisplayChanged(bool bShow);
	void RefreshDisplay();
	void ShowFPSDisplay();
	void CleanupFPSDisplay();
	UPGRuntimePreloadSubsystem* GetRuntimePreloadSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<APGPlayerController> OwnerPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UPGFPSDisplayWidget> FPSDisplayWidget;
};
