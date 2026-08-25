// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PGRunDirector.generated.h"

class APGGameModeBase;
class APGRunnerCharacter;
class APGTrackSegment;
class UPGTrackSegmentSet;

/** 클래스별로 비활성 트랙 세그먼트를 보관하는 런타임 풀입니다. */
struct FPGPooledTrackSegments
{
	// World가 수명을 소유하는 비활성 Actor를 풀에서 관찰하며 파괴된 항목은 획득 시 건너뜁니다.
	TArray<TWeakObjectPtr<APGTrackSegment>> Segments;
};

/**
 * 현재 월드에 활성화된 트랙 세그먼트의 진행 거리 범위와 배치 상태입니다.
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
 * 무한 러닝 트랙 세그먼트의 생성, 배치, 회수, 풀링을 담당하는 진행 관리자입니다.
 * 환경 위협 이벤트는 RunEventSubsystem이 별도로 담당하고, 이 Actor는 트랙 유지 범위만 관리합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGRunDirector : public AActor
{
	GENERATED_BODY()

public:
	/** Home/Failed 상태에서는 Tick하지 않고 Countdown 이후 0.1초 간격으로 트랙을 관리하도록 구성합니다. */
	APGRunDirector();

	/** 레벨 배치 중복을 검증하고 시작 세그먼트·GameMode·러너 참조를 연결합니다. */
	virtual void BeginPlay() override;

	/** GameMode delegate와 활성·풀링 세그먼트 참조를 월드 종료 전에 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 활성 런에서 지나간 세그먼트를 회수하고 전방 목표 거리까지 새 세그먼트를 채웁니다. */
	virtual void Tick(float DeltaSeconds) override;

	/** 러닝 시간이 난이도 램프 시간 대비 얼마나 진행됐는지 0~1로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Director")
	float GetDifficultyAlpha() const;

	/** 현재 월드에 배치됐거나 풀에서 꺼내 활성화한 트랙 세그먼트 수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Run Director")
	int32 GetActiveSegmentCount() const { return ActiveSegments.Num(); }

	/** 맵 계약 검증에 사용하는 레벨 배치 시작 세그먼트를 반환하며 누락 시 null입니다. */
	APGTrackSegment* GetInitialSegment() const { return InitialSegment; }
	/** 가중치 트랙 후보 카탈로그를 반환하며 미설정이면 null입니다. */
	UPGTrackSegmentSet* GetSegmentSet() const { return SegmentSet; }

protected:
	// 자동 생성할 트랙 세그먼트 후보 DataAsset입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track")
	TObjectPtr<UPGTrackSegmentSet> SegmentSet;

	// 레벨에 직접 배치한 필수 시작 세그먼트입니다. 누락되면 맵 계약 검증에서 START를 차단합니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Run Director | Track")
	TObjectPtr<APGTrackSegment> InitialSegment;

	// 플레이어 앞쪽에 유지할 목표 활성 세그먼트 수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "2", UIMin = "2", UIMax = "12"))
	int32 TargetActiveSegmentCount = 6;

	// 플레이어 진행 방향 앞쪽으로 최소 유지할 거리(cm)이며 500 미만 입력은 clamp됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "500.0", UIMin = "1000.0", UIMax = "20000.0"))
	float SpawnAheadDistance = 8000.0f;

	// 플레이어가 세그먼트 끝을 이 거리(cm)만큼 지나면 풀로 회수합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "0.0", UIMin = "500.0", UIMax = "10000.0"))
	float DespawnBehindDistance = 2500.0f;

	// 사용자가 레벨에 직접 배치한 시작 세그먼트도 지나간 뒤 Destroy할지 여부입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track")
	bool bDestroyPassedPlacedSegments = false;

	// 클래스별로 보관할 최대 비활성 세그먼트 수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Track", meta = (ClampMin = "0", UIMin = "0", UIMax = "20"))
	int32 MaxPooledSegmentsPerClass = 8;

	// 트랙 난이도가 1.0까지 도달하는 초 단위 시간이며 최소 1초로 clamp됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Difficulty", meta = (ClampMin = "1.0", UIMin = "30.0", UIMax = "300.0"))
	float DifficultyRampDuration = 180.0f;

	// 같은 트랙 생성 순서를 재현할 때 사용하는 시드입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Random")
	int32 RandomSeed = 1337;

	// 켜면 BeginPlay마다 새 랜덤 시드를 사용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Director | Random")
	bool bUseRandomSeedEachRun = true;

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
	// 실제 spawn 성공 이력만 fallback으로 기억하며 클래스 수명은 SegmentSet DataAsset이 유지합니다.
	TWeakObjectPtr<UClass> LastValidSegmentClass;
};
