// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/PGTrackPoolableInterface.h"
#include "PGReceiptPickupActor.generated.h"

class UNiagaraSystem;
class USoundBase;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 트랙에 직접 배치해 러닝 중 영수증 재화를 획득하게 하는 수집 액터입니다.
 * 세그먼트 풀링에 맞춰 획득 후 Destroy하지 않고 비활성화했다가 재활성화 시 상태를 복구합니다.
 */
UCLASS(Blueprintable)
class PROJECTG_API APGReceiptPickupActor : public AActor, public IPGTrackPoolableInterface
{
	GENERATED_BODY()

public:
	APGReceiptPickupActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 세그먼트가 풀에서 재사용될 때 영수증을 다시 획득 가능한 상태로 복구합니다.
	virtual void OnTrackSegmentActivatedFromPool_Implementation() override;

	// 세그먼트가 풀로 반환될 때 충돌과 표시를 꺼서 비활성 상태로 정리합니다.
	virtual void OnTrackSegmentDeactivatedForPool_Implementation() override;

protected:
	// 영수증 액터의 루트입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Receipt Pickup")
	TObjectPtr<USceneComponent> PickupRoot;

	// 러너가 닿으면 영수증을 획득하는 트리거입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Receipt Pickup")
	TObjectPtr<USphereComponent> PickupTrigger;

	// 에디터에서 교체 가능한 영수증 표시 메시입니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Receipt Pickup")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	// 획득 시 추가할 영수증 개수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Receipt Pickup", meta = (ClampMin = "1", UIMin = "1", UIMax = "100"))
	int32 ReceiptAmount = 1;

	// 획득 순간 한 번 재생할 Niagara VFX입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Receipt Pickup | Feedback")
	TObjectPtr<UNiagaraSystem> PickupVFX;

	// 획득 순간 한 번 재생할 SFX입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Receipt Pickup | Feedback")
	TObjectPtr<USoundBase> PickupSound;

private:
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void SetPickupActive(bool bActive);
	void PlayPickupFeedback() const;
	bool CanCollectFromActor(const AActor* OtherActor) const;

	bool bCollected = false;
};
