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
	UPGRunResultFeedbackComponent();

	virtual void BeginPlay() override;
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

	UPROPERTY(Transient)
	TObjectPtr<APGGameModeBase> BoundGameMode;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultWidget;

	bool bIsBoundToGameMode = false;
};
