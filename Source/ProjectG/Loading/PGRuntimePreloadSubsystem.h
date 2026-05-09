// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGRuntimePreloadSubsystem.generated.h"

struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPGRuntimePreloadCompletedSignature);

/**
 * 홈/러닝/결과 화면에서 즉시 필요한 UI, VFX, SFX, 적재물 데이터를 게임 시작 전에 비동기로 예열하는 서브시스템입니다.
 * 런타임 중 리소스가 늦게 보여지는 상황을 막기 위해 Soft Reference를 중앙에서 한 번 로드하고 핸들을 유지합니다.
 */
UCLASS()
class PROJECTG_API UPGRuntimePreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 필요한 런타임 리소스 예열을 시작합니다. 이미 완료되었거나 진행 중이면 중복 실행하지 않습니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Runtime Preload")
	void StartPreload();

	// 홈에서 러닝으로 넘어가기 전에 필수 리소스가 준비됐는지 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runtime Preload")
	bool IsReady() const { return bIsReady; }

	// 현재 비동기 예열이 진행 중인지 반환합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runtime Preload")
	bool IsPreloading() const { return bIsPreloading; }

	// 모든 예열이 끝났을 때 한 번 방송됩니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Runtime Preload")
	FPGRuntimePreloadCompletedSignature OnPreloadCompleted;

private:
	void CollectPrimaryResourcePaths(TArray<FSoftObjectPath>& OutPaths) const;
	void CollectNestedResourcePaths(TArray<FSoftObjectPath>& OutPaths) const;
	void AddUniquePath(TArray<FSoftObjectPath>& OutPaths, const FSoftObjectPath& Path) const;
	void HandlePrimaryResourcesLoaded();
	void HandleNestedResourcesLoaded();
	void CompletePreload();
	void ReleaseHandles();

	TSharedPtr<FStreamableHandle> PrimaryLoadHandle;
	TSharedPtr<FStreamableHandle> NestedLoadHandle;

	bool bIsReady = false;
	bool bIsPreloading = false;
};
