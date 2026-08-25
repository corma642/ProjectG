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
	/** 이벤트 delegate로만 구동되도록 ActorComponent Tick을 비활성화합니다. */
	UPGRunEventFeedbackComponent();

	/** 로컬 RunEventSubsystem과 사용자 SFX 설정을 연결하고 presentation 리소스를 검증합니다. */
	virtual void BeginPlay() override;
	/** 모든 delegate와 생성한 위젯·Niagara·루프 오디오를 컴포넌트 종료 전에 정리합니다. */
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

	void HandleSFXVolumeChanged(float SFXVolumeScale);
	void ResolvePresentationSet();
	bool ArePresentationResourcesReady() const;
	void ValidatePresentationSet() const;
	void BindRunEventSubsystem();
	void UnbindRunEventSubsystem();
	void BindUserSettings();
	void UnbindUserSettings();
	const FPGRunEventPresentationEntry* FindPresentationEntry(EPGRunEventType EventType);
	void PlayPreviewPresentation(const FPGRunEventPresentationEntry& Entry, EPGRunEventType EventType, float TimeUntilTrigger, float Direction);
	void PlayActivePresentation(const FPGRunEventPresentationEntry& Entry, EPGRunEventType EventType, float Duration, float Direction);
	void CleanupPreviewPresentation();
	void CleanupActivePresentation();
	void CleanupAllPresentations();
	APlayerController* GetOwningPlayerController() const;
	USceneComponent* GetVfxAttachComponent() const;

	// 설정에서 로드한 DataAsset은 활성 연출이 참조하는 동안 생존시킵니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRunEventPresentationSet> PresentationSet;

	// 현재 월드가 소유하는 Subsystem을 delegate 수명 동안 관찰합니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGRunEventSubsystem> BoundRunEventSubsystem;

	// viewport에 추가한 예고 위젯은 종료·취소 시 명시적으로 제거합니다.
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PreviewWidget;

	// viewport에 추가한 발동 위젯은 이벤트 종료·취소 시 명시적으로 제거합니다.
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
