// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cargo/PGCargoPresentationSet.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "PGCargoFeedbackComponent.generated.h"

class APGRunnerCharacter;
class APlayerController;
class UAudioComponent;
class UNiagaraComponent;
class UPGCargoPresentationSet;
class UPGCargoStackComponent;
class UUserWidget;
struct FStreamableHandle;

/**
 * 로컬 플레이어의 적재물 관련 UI/VFX/SFX를 자동 실행하는 컴포넌트입니다.
 * CargoStackComponent의 내부 신호를 구독하고 PresentationSet 리소스를 재생/정리합니다.
 */
UCLASS(ClassGroup = (ProjectG), meta = (BlueprintSpawnableComponent))
class PROJECTG_API UPGCargoFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGCargoFeedbackComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// PlayerController가 새 러너를 소유했을 때 적재물 신호를 자동으로 연결합니다.
	void HandleRunnerPossessed(APGRunnerCharacter* RunnerCharacter);

	// PlayerController가 러너 소유를 잃었을 때 적재물 신호와 연출을 정리합니다.
	void HandleRunnerUnpossessed();

private:
	void HandleCargoDropHoldStarted();
	void HandleCargoDropHoldFinished();
	void HandleCargoDelivered(int32 CargoInstanceId, int32 Score, int32 Receipts);
	void HandleCargoLost(int32 CargoIndex);

	void StartLoadingPresentationSet();
	void HandlePresentationSetLoaded();
	void StartPreloadingPresentationResources();
	void HandlePresentationResourcesLoaded();
	void ValidatePresentationSet() const;
	void ReleasePresentationLoadHandles();
	void BindCargoStack(UPGCargoStackComponent* CargoStackComponent);
	void UnbindCargoStack();
	const FPGCargoPresentationEntry* FindPresentationEntry(EPGCargoFeedbackType FeedbackType) const;
	void PlayPersistentPresentation(const FPGCargoPresentationEntry& Entry, EPGCargoFeedbackType FeedbackType, int32 CargoIndex, float DisplayDuration);
	void PlayTransientPresentation(const FPGCargoPresentationEntry& Entry, EPGCargoFeedbackType FeedbackType, int32 CargoIndex);
	void CleanupPersistentPresentation();
	void CleanupTransientPresentation();
	void CleanupAllPresentations();
	APlayerController* GetOwningPlayerController() const;
	USceneComponent* GetVfxAttachComponent() const;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoPresentationSet> PresentationSet;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> BoundCargoStackComponent;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PersistentWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TransientWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> PersistentNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TransientNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PersistentLoopAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TransientLoopAudioComponent;

	TSharedPtr<FStreamableHandle> PresentationSetLoadHandle;
	TSharedPtr<FStreamableHandle> PresentationResourcesLoadHandle;
	FTimerHandle TransientCleanupTimerHandle;

	EPGCargoFeedbackType PersistentFeedbackType = EPGCargoFeedbackType::None;
	bool bPresentationSetLoaded = false;
	bool bPresentationResourcesReady = false;
};
