// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGRuntimePreloadSubsystem.generated.h"

struct FStreamableHandle;

/**
 * 앱 실행 중 필요한 리소스를 화면 흐름별로 나눠 비동기 예열합니다.
 * Home 표시 전에 대형 상점/튜토리얼 리소스를 한꺼번에 올리지 않아 저사양 Android의 시작 메모리 피크를 줄입니다.
 */
UENUM(BlueprintType)
enum class EPGRuntimePreloadGroup : uint8
{
	Home,
	Run,
	Shop,
	Tutorial
};

/** 요청한 리소스 그룹의 비동기 로드가 끝나 대기 중인 화면 흐름을 재개할 때 발생합니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPGRuntimePreloadCompletedSignature, EPGRuntimePreloadGroup, Group);

/**
 * UI, VFX, SFX, 적재물 데이터를 화면 흐름별로 비동기 예열하는 GameInstance Subsystem입니다.
 * Home은 앱 시작 시 준비하고, Run/Shop/Tutorial은 실제 사용 직전에 요청합니다.
 */
UCLASS()
class PROJECTG_API UPGRuntimePreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 맵 전환 delegate를 연결하고 Home 그룹의 비동기 예열 상태를 받을 준비를 합니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 진행 중인 streamable handle과 맵 delegate를 Subsystem 종료 전에 해제합니다. */
	virtual void Deinitialize() override;

	/** 지정한 화면 흐름에 필요한 리소스 예열을 시작합니다. 이미 준비됐거나 진행 중이면 중복 요청하지 않습니다. */
	void RequestPreload(EPGRuntimePreloadGroup Group);

	/** 지정한 화면 흐름의 예열 완료 여부를 반환합니다. */
	bool IsGroupReady(EPGRuntimePreloadGroup Group) const;

	/** 지정한 화면 흐름의 예열 진행 여부를 반환합니다. */
	bool IsGroupPreloading(EPGRuntimePreloadGroup Group) const;

	/** 카운트다운을 시작하기 전에 Run 리소스와 현재 맵 적재물 후보가 모두 준비됐는지 확인합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runtime Preload")
	bool IsReadyForRun() const;

	/** START가 차단된 원인을 조치 가능한 설정 경고로 출력합니다. */
	void LogRunReadinessIssues() const;

	/**
	 * 동일 레벨 Retry 직전에 호출해 다음 맵 로드 1회 동안 준비된 Run 리소스를 유지합니다.
	 * 다른 맵으로 이동하거나 Run 예열이 완료되지 않은 상태라면 기존처럼 리소스를 해제합니다.
	 */
	void PreserveRunResourcesForNextSameMapTravel(FName ExpectedLevelName);

	/** 화면 흐름 하나의 예열이 완료될 때마다 해당 그룹을 전달합니다. */
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Runtime Preload")
	FPGRuntimePreloadCompletedSignature OnPreloadCompleted;

private:
	struct FPGPreloadGroupState
	{
		TSharedPtr<FStreamableHandle> PrimaryLoadHandle;
		TSharedPtr<FStreamableHandle> NestedLoadHandle;
		TArray<FSoftObjectPath> RequestedResourcePaths;
		bool bIsReady = false;
		bool bIsPreloading = false;
	};

	FPGPreloadGroupState& GetOrAddGroupState(EPGRuntimePreloadGroup Group);
	const FPGPreloadGroupState* FindGroupState(EPGRuntimePreloadGroup Group) const;
	void CollectPrimaryResourcePaths(EPGRuntimePreloadGroup Group, TArray<FSoftObjectPath>& OutPaths) const;
	void CollectNestedResourcePaths(EPGRuntimePreloadGroup Group, TArray<FSoftObjectPath>& OutPaths) const;
	void AddUniquePath(TArray<FSoftObjectPath>& OutPaths, const FSoftObjectPath& Path) const;
	void HandlePrimaryResourcesLoaded(EPGRuntimePreloadGroup Group);
	void HandleNestedResourcesLoaded(EPGRuntimePreloadGroup Group);
	void CompletePreload(EPGRuntimePreloadGroup Group);
	void HandlePreLoadMap(const FString& MapName);
	void ReleaseGroupHandles(FPGPreloadGroupState& GroupState);
	void ReleaseHandles();
	void ValidateLoadedResources(EPGRuntimePreloadGroup Group);
	bool AreRequiredRunResourcesReady(bool bLogIssues) const;
	bool IsCurrentMapCargoDropSetReady(bool bLogIssues) const;

	TMap<EPGRuntimePreloadGroup, FPGPreloadGroupState> GroupStates;
	FName PreservedRunResourcesExpectedLevelName = NAME_None;
};
