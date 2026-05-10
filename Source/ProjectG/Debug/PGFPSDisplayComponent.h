// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGFPSDisplayComponent.generated.h"

class APGPlayerController;
class UPGFPSDisplayWidget;
class UPGRuntimePreloadSubsystem;

/**
 * 로컬 플레이어의 FPS 표시 위젯을 Settings 저장값에 맞춰 생성/제거하는 독립 컴포넌트입니다.
 * 게임 상태와 무관하게 Android 프레임 테스트용 오버레이만 관리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGFPSDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGFPSDisplayComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRuntimePreloadCompleted();

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
