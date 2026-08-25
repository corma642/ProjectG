// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PGRunResultFeedbackComponent.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;
class APlayerController;
class UUserWidget;

/**
 * GameMode의 공통 실패 이벤트를 구독해 로컬 플레이어 결과 화면을 자동으로 표시하는 컴포넌트입니다.
 * 종료 UI 리소스는 Project Settings에서 지정하고, 런 시작 시 비동기로 예열합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunResultFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 실패 delegate로만 구동되도록 ActorComponent Tick을 비활성화합니다. */
	UPGRunResultFeedbackComponent();

	/** 로컬 GameMode 실패 delegate를 연결하고 기존 결과 위젯이 없는 초기 상태를 준비합니다. */
	virtual void BeginPlay() override;
	/** GameMode delegate와 viewport 결과 위젯을 컴포넌트 종료 전에 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	bool AreRuntimeResourcesReady() const;
	void BindGameMode();
	void UnbindGameMode();
	void CleanupResultWidget();
	APlayerController* GetOwningPlayerController() const;
	APGRunnerCharacter* GetRunnerCharacter() const;

	// 실패 상태를 발생시키는 현재 월드 GameMode를 delegate 수명 동안 유지합니다.
	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	// viewport에 표시한 결과 위젯은 재시도·Home·월드 종료 시 명시적으로 제거합니다.
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultWidget;

	bool bIsBoundToGameMode = false;
};
