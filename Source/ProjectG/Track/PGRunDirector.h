// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/PGTrackEventMarkerComponent.h"
#include "Track/PGTrackSegmentSet.h"
#include "PGRunDirector.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;
class UPGCargoStackComponent;

struct FPGPooledTrackSegments
{
	TArray<TObjectPtr<APGTrackSegment>> Segments;
};

/**
 * 현재 런에서 활성화된 세그먼트의 런타임 상태입니다.
 * 진행 거리 기준 회수와 디버그 표시를 위해 시작/끝 진행값을 캐시합니다.
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
 * 무한 러닝 길 생성, 세그먼트 회수, 난이도 증가, 자동 이벤트 배정을 담당하는 진행 관리자입니다.
 * 레벨에는 하나만 배치하고, 시작 세그먼트 이후의 길은 이 Actor가 직선 진행 방향으로 이어 붙입니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGRunDirector : public AActor
{
	GENERATED_BODY()

public:
	APGRunDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 현재 난이도 진행도입니다. 0은 시작 직후, 1은 설정한 램프 시간이 지난 상태입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Director")
	float GetDifficultyAlpha() const;

	// 현재 런에서 생성 또는 등록된 활성 세그먼트 수입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Director")
	int32 GetActiveSegmentCount() const { return ActiveSegments.Num(); }

protected:
	// 자동 생성 후보 목록 Data Asset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track")
	TObjectPtr<UPGTrackSegmentSet> SegmentSet;

	// 사용자가 레벨에 직접 배치한 시작 세그먼트. 비워두면 첫 후보 세그먼트를 Director 위치에 생성합니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Run Director | Track")
	TObjectPtr<APGTrackSegment> InitialSegment;

	// 앞쪽에 유지할 목표 세그먼트 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "2", UIMin = "2", UIMax = "12"))
	int32 TargetActiveSegmentCount = 6;

	// 플레이어 앞쪽으로 최소 유지할 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "500.0", UIMin = "1000.0", UIMax = "20000.0"))
	float SpawnAheadDistance = 8000.0f;

	// 플레이어가 세그먼트 끝을 이 거리만큼 지나면 회수합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "0.0", UIMin = "500.0", UIMax = "10000.0"))
	float DespawnBehindDistance = 2500.0f;

	// 레벨에 직접 배치한 시작 세그먼트도 지나간 뒤 Destroy할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track")
	bool bDestroyPassedPlacedSegments = false;

	// 클래스별 풀에 보관할 최대 비활성 세그먼트 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "0", UIMin = "0", UIMax = "20"))
	int32 MaxPooledSegmentsPerClass = 8;

	// 난이도 1.0까지 도달하는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Difficulty", meta = (ClampMin = "1.0", UIMin = "30.0", UIMax = "300.0"))
	float DifficultyRampDuration = 180.0f;

	// 초반 안전 구간으로 자동 이벤트를 붙이지 않을 세그먼트 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Difficulty", meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 SafeInitialGeneratedSegments = 2;

	// 초반 안전 구간으로 자동 이벤트를 막을 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Difficulty", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
	float SafeInitialRunTime = 10.0f;

	// 초반 안전 구간으로 자동 이벤트를 막을 진행 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Difficulty", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10000.0"))
	float SafeInitialDistance = 2500.0f;

	// 난이도 0에서 세그먼트에 이벤트가 붙을 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BaseEventChance = 0.05f;

	// 난이도 1에서 세그먼트에 이벤트가 붙을 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MaxEventChance = 0.55f;

	// 이벤트가 너무 촘촘하게 나오지 않도록 필요한 최소 세그먼트 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0", UIMin = "0", UIMax = "10"))
	int32 MinSegmentsBetweenEvents = 1;

	// 이벤트가 너무 촘촘하게 나오지 않도록 필요한 최소 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "30.0"))
	float MinSecondsBetweenEvents = 4.0f;

	// 이벤트가 너무 촘촘하게 나오지 않도록 필요한 최소 진행 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10000.0"))
	float MinDistanceBetweenEvents = 1800.0f;

	// 바람 이벤트가 등장하기 시작하는 난이도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WindMinDifficulty = 0.1f;

	// 얼음 이벤트가 등장하기 시작하는 난이도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float IceMinDifficulty = 0.25f;

	// 추가 적재 이벤트가 등장하기 시작하는 난이도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Events", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float CargoDropMinDifficulty = 0.2f;

	// 런마다 같은 순서를 재현할 때 사용하는 시드
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Random")
	int32 RandomSeed = 1337;

	// 켜면 BeginPlay마다 랜덤 시드를 새로 사용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Random")
	bool bUseRandomSeedEachRun = true;

	// 개발 빌드에서 세그먼트/난이도 상태를 화면에 표시합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Debug")
	bool bDrawRunDirectorDebug = false;

private:
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
	void ConfigureGeneratedEvents(APGTrackSegment* Segment, const FPGTrackSegmentEntry& SegmentEntry, int32 SegmentIndex);
	int32 SelectSegmentEntryIndex();
	TSubclassOf<APGTrackSegment> ResolveFallbackSegmentClass() const;
	bool CanAssignGeneratedEvent(int32 SegmentIndex, float SegmentProgress) const;
	EPGTrackEventType SelectEventType(float DifficultyAlpha) const;
	bool IsCargoDropAvailable() const;
	float GetPlayerProgress() const;
	float GetProgressAtLocation(const FVector& WorldLocation) const;
	void DrawRunDirectorDebug() const;

	UPROPERTY(Transient)
	TObjectPtr<APGRunnerCharacter> RunnerCharacter;

	UPROPERTY(Transient)
	TArray<FPGActiveTrackSegment> ActiveSegments;

	UPROPERTY(Transient)
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	TMap<TSubclassOf<APGTrackSegment>, FPGPooledTrackSegments> SegmentPoolByClass;
	FRandomStream RandomStream;
	FVector RunOrigin = FVector::ZeroVector;
	FVector RunForward = FVector::ForwardVector;
	float RunElapsedTime = 0.0f;
	bool bIsRunActive = true;
	int32 GeneratedSegmentCount = 0;
	int32 LastSelectedEntryIndex = INDEX_NONE;
	int32 ConsecutiveEntryCount = 0;
	int32 LastEventSegmentIndex = INDEX_NONE;
	float LastEventTime = -BIG_NUMBER;
	float LastEventProgress = -BIG_NUMBER;
	TMap<int32, int32> LastSpawnedSegmentByEntry;
	TSubclassOf<APGTrackSegment> LastValidSegmentClass;
};
