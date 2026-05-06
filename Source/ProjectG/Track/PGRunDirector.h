// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/PGTrackSegmentSet.h"
#include "PGRunDirector.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;

struct FPGPooledTrackSegments
{
	TArray<TObjectPtr<APGTrackSegment>> Segments;
};

/**
 * 현재 월드에 활성화된 트랙 세그먼트의 진행 범위와 배치 상태입니다.
 * 플레이어 진행 거리 기준 회수와 디버그 표시를 위해 시작/끝 진행값을 캐시합니다.
 */
USTRUCT()
struct FPGActiveTrackSegment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APGTrackSegment> Segment = nullptr;

	UPROPERTY()
	int32 EntryIndex = INDEX_NONE;

	UPROPERTY()
	float StartProgress = 0.0f;

	UPROPERTY()
	float EndProgress = 0.0f;

	UPROPERTY()
	bool bPlacedInLevel = false;
};

/**
 * 무한 러닝 트랙 세그먼트 생성과 회수만 담당하는 진행 관리자입니다.
 * 환경 이벤트는 UPGRunEventSubsystem이 러닝 시간 기준으로 별도 관리합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGRunDirector : public AActor
{
	GENERATED_BODY()

public:
	APGRunDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 현재 시간 기반 난이도입니다. 0은 시작 직후, 1은 설정한 램프 시간이 지난 상태입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Director")
	float GetDifficultyAlpha() const;

	// 현재 월드에 생성 또는 풀에서 활성화된 트랙 세그먼트 수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Director")
	int32 GetActiveSegmentCount() const { return ActiveSegments.Num(); }

protected:
	// 자동 생성 후보 트랙 세그먼트 DataAsset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track")
	TObjectPtr<UPGTrackSegmentSet> SegmentSet;

	// 레벨에 직접 배치한 시작 세그먼트. 비워두면 첫 번째 발견 세그먼트를 사용합니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Run Director | Track")
	TObjectPtr<APGTrackSegment> InitialSegment;

	// 플레이어 앞쪽에 유지할 목표 세그먼트 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "2", UIMin = "2", UIMax = "12"))
	int32 TargetActiveSegmentCount = 6;

	// 플레이어 앞쪽으로 최소 유지할 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "500.0", UIMin = "1000.0", UIMax = "20000.0"))
	float SpawnAheadDistance = 8000.0f;

	// 플레이어가 세그먼트 끝을 이 거리만큼 지나면 회수합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "0.0", UIMin = "500.0", UIMax = "10000.0"))
	float DespawnBehindDistance = 2500.0f;

	// 사용자가 직접 배치한 시작 세그먼트도 지나간 뒤 Destroy할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track")
	bool bDestroyPassedPlacedSegments = false;

	// 클래스별 풀에 보관할 최대 비활성 세그먼트 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "0", UIMin = "0", UIMax = "20"))
	int32 MaxPooledSegmentsPerClass = 8;

	// 난이도가 1.0까지 도달하는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Difficulty", meta = (ClampMin = "1.0", UIMin = "30.0", UIMax = "300.0"))
	float DifficultyRampDuration = 180.0f;

	// 같은 순서를 재현할 때 사용하는 시드입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Random")
	int32 RandomSeed = 1337;

	// 켜면 BeginPlay마다 새 랜덤 시드를 사용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Random")
	bool bUseRandomSeedEachRun = true;

	// 개발 빌드에서 세그먼트와 난이도 상태를 화면에 표시합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Debug")
	bool bDrawRunDirectorDebug = false;

private:
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	UFUNCTION()
	void HandleRunStarted();

	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	void InitializeRunnerReference();
	void InitializeInitialSegment();
	void ValidateTrackData() const;
	void SpawnSegmentsIfNeeded();
	bool SpawnNextSegment();
	void RemovePassedSegments();
	APGTrackSegment* AcquireSegment(TSubclassOf<APGTrackSegment> SegmentClass, const FTransform& SpawnTransform);
	void ReturnSegmentToPool(APGTrackSegment* Segment);
	int32 SelectSegmentEntryIndex();
	TSubclassOf<APGTrackSegment> ResolveFallbackSegmentClass() const;
	float GetPlayerProgress() const;
	float GetProgressAtLocation(const FVector& WorldLocation) const;
	void DrawRunDirectorDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<APGRunnerCharacter> RunnerCharacter;

	UPROPERTY(Transient)
	TArray<FPGActiveTrackSegment> ActiveSegments;

	TMap<TSubclassOf<APGTrackSegment>, FPGPooledTrackSegments> SegmentPoolByClass;
	FRandomStream RandomStream;
	FVector RunOrigin = FVector::ZeroVector;
	FVector RunForward = FVector::ForwardVector;
	float RunElapsedTime = 0.0f;
	bool bIsRunActive = false;
	bool bIsDifficultyActive = false;
	int32 GeneratedSegmentCount = 0;
	int32 LastSelectedEntryIndex = INDEX_NONE;
	int32 ConsecutiveEntryCount = 0;
	TMap<int32, int32> LastSpawnedSegmentByEntry;
	TSubclassOf<APGTrackSegment> LastValidSegmentClass;
};
