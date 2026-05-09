// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RunEvents/PGRunEventPresentationSet.h"
#include "PGRunEventFeedbackComponent.generated.h"

class APlayerController;
class UNiagaraComponent;
class UPGRunEventSubsystem;
class UAudioComponent;
class UUserWidget;

/**
 * 로컬 플레이어의 런 이벤트 UI/VFX/SFX를 자동 실행하는 컴포넌트입니다.
 * RunEventSubsystem의 상태 이벤트를 구독하고 PresentationSet 리소스를 재생/정리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGRunEventFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGRunEventFeedbackComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRunEventPreviewStarted(EPGRunEventType EventType, float TimeUntilTrigger, float Direction);

	UFUNCTION()
	void HandleRunEventTriggered(EPGRunEventType EventType, float Duration, float Direction);

	UFUNCTION()
	void HandleRunEventFinished(EPGRunEventType EventType);

	UFUNCTION()
	void HandleRunEventCanceled(EPGRunEventType EventType);

	void ResolvePresentationSet();
	bool ArePresentationResourcesReady() const;
	void ValidatePresentationSet() const;
	void BindRunEventSubsystem();
	void UnbindRunEventSubsystem();
	const FPGRunEventPresentationEntry* FindPresentationEntry(EPGRunEventType EventType);
	void PlayPreviewPresentation(const FPGRunEventPresentationEntry& Entry, EPGRunEventType EventType, float TimeUntilTrigger, float Direction);
	void PlayActivePresentation(const FPGRunEventPresentationEntry& Entry, EPGRunEventType EventType, float Duration, float Direction);
	void CleanupPreviewPresentation();
	void CleanupActivePresentation();
	void CleanupAllPresentations();
	APlayerController* GetOwningPlayerController() const;
	USceneComponent* GetVfxAttachComponent() const;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunEventPresentationSet> PresentationSet;

	UPROPERTY(Transient)
	TObjectPtr<UPGRunEventSubsystem> BoundRunEventSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PreviewWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> PreviewNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveLoopAudioComponent;

	EPGRunEventType PreviewEventType = EPGRunEventType::None;
	EPGRunEventType ActiveEventType = EPGRunEventType::None;
};
