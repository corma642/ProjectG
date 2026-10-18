// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Settings/PGGameSettings.h"
#include "PGRunnerCharacter.generated.h"

class UCameraComponent;
class UPGCharacterSkinDefinition;
class UPGCargoStackComponent;
class UPGRunnerEnvironmentComponent;
class UPGRunnerMovementComponent;
class UPGRunProgressComponent;
class USpringArmComponent;

/**
 * 자동 전진, 터치 점프, 중심 보정, 적재물 반응을 연결하는 러너 캐릭터입니다.
 * 실제 좌우 이동은 전용 MovementComponent의 밸런스 오프셋이 담당하고, 적재물 물리는 CargoStackComponent가 담당합니다.
 */
UCLASS()
class PROJECTG_API APGRunnerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** 러너 전용 MovementComponent와 카메라·적재물·환경·진행 컴포넌트를 기본 서브오브젝트로 구성합니다. */
	APGRunnerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Countdown/Running에서만 자동 전진, 중심 불안정성, 적재물 반응과 추락 실패 판정을 갱신합니다. */
	virtual void Tick(float DeltaSeconds) override;

	/** 시작 위치와 Project Settings 값을 캐시해 Home 복구와 이후 런 reset의 기준을 준비합니다. */
	virtual void BeginPlay() override;

	/** 런 중 생성한 Bowl Actor와 delegate가 레벨 전환 또는 재시작 뒤 남지 않도록 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 위쪽 플릭 또는 데스크톱 점프 키의 수직 점프를 요청하고, 실제로 점프가 시작됐는지 반환합니다. */
	bool RequestJump();

	/** -1~1 방향 부호의 플릭 입력으로 점프와 짧은 측면 이동 보정을 함께 요청합니다. */
	void RequestFlickJump(float DirectionSign);

	/** 터치 드래그 또는 키보드에서 계산한 중심 보정 입력을 -1~1 범위로 반영합니다. */
	void SetBalanceInput(float InBalanceInput);

	/** 현재 플레이어가 입력 중인 좌우 중심 보정값을 -1~1 범위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetBalanceInput() const { return BalanceInput; }

	/** 현재 런이 시작된 뒤 경과한 시간을 초 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetRunElapsedTime() const;

	/** 런 시작 지점 기준 전방 진행 거리를 UI 표시용 미터 단위로 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	float GetRunDistanceMeters() const;

	/** RunProgressComponent가 계산한 현재 런 점수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetRunScore() const;

	/** 현재 런에서 획득한 영수증 수를 반환합니다. 영구 지갑 잔액과는 별개입니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetRunReceipts() const;

	/** 현재 런에서 배달 완료한 적재물 개수를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "ProjectG | Runner")
	int32 GetDeliveredCargoCount() const;

	/** 적재물 드롭과 HUD가 현재 러너의 적재 상태에 접근할 때 사용하는 컴포넌트를 반환합니다. */
	UPGCargoStackComponent* GetCargoStackComponent() const { return CargoStackComponent; }

	/** 런 중 머리에 붙어 있는 바구니 Actor입니다. 카운트다운 전·실패 후에는 null입니다. */
	AActor* GetAttachedBowlActor() const { return AttachedBowlActor; }

	/** HUD가 점수, 거리, 영수증 변화를 구독할 때 사용하는 진행 컴포넌트를 반환합니다. */
	UPGRunProgressComponent* GetRunProgressComponent() const { return RunProgressComponent; }

	/** 선택한 스킨의 메시·애님 클래스를 적용하고, 필수 에셋이 없으면 false를 반환합니다. */
	bool ApplyCharacterSkin(const UPGCharacterSkinDefinition* SkinDefinition);

	/** Home/Shop 프리뷰 회전과 분리해 실제 런 시작을 yaw 0 기준으로 정렬합니다. ForwardOffset(cm)만큼 월드 +X로 옮겨 시작합니다(Retry의 짧은 카운트다운 보정). */
	void PrepareTransformForRunStart(float ForwardOffset = 0.0f);

	/** Home 화면에 다시 표시될 때 BeginPlay에서 저장한 배치 Transform으로 복구합니다. */
	void RestoreHomePreviewTransform();

	/** 물리 적재물이 손실됐을 때 GameMode에 런 실패를 요청합니다. */
	UFUNCTION()
	void HandleCargoLost(int32 CargoIndex);

	/** 적재물 배달 완료 점수·영수증을 진행도에 반영하고 월드 위치 피드백을 전달합니다. */
	void HandleCargoDelivered(int32 CargoInstanceId, int32 Score, int32 Receipts, FVector DeliveryLocation);

	/** 런 실패 이벤트를 받으면 자동 전진·입력·Actor Tick을 정지하고 실패 lean을 적용합니다. */
	UFUNCTION()
	void HandleRunFailed(FName FailureReason);

	/** 카운트다운 시작 시 러너 Transform과 적재 상태를 새 런 기준으로 준비하고 Tick을 활성화합니다. */
	UFUNCTION()
	void HandleRunCountdownStarted(float Duration);

	/** GO 시점 위치·시간을 점수 기준으로 다시 잡고 입력과 환경 반응을 활성화합니다. */
	UFUNCTION()
	void HandleRunStarted();

	/** 실제 점프가 시작된 직후 발생합니다. 첫 런 가이드가 플릭 단계 완료 판정에 사용합니다. */
	FSimpleMulticastDelegate OnJumpStarted;

	/** 결과 화면이 뜰 때 러너 메시를 숨기고 바구니를 제거합니다. 실패 직후 슬로모션 동안은 보이게 둬 실패 원인을 보여 줍니다. */
	void HideForRunResult();

protected:
	// 점프와 캡슐 좌우 밸런스 이동을 담당하는 러너 전용 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<UPGRunnerMovementComponent> RunnerMovementComponent;

	// 런 시작 시 머리 소켓에 스폰/부착할 바구니 Actor 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	TSubclassOf<AActor> BowlActorClass;

	// 바구니 부착 소켓 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo")
	FName BowlSocketName = FName(TEXT("s_HeadCargoSocket"));

	// 물리 적재물 생성/대기/손실 상태를 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo")
	TObjectPtr<UPGCargoStackComponent> CargoStackComponent;

	// 바람/눈바람처럼 러너 조작감에 영향을 주는 환경 이벤트 상태를 관리하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run Event")
	TObjectPtr<UPGRunnerEnvironmentComponent> RunnerEnvironmentComponent;

	// 런 중 진행 거리와 점수를 추적하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TObjectPtr<UPGRunProgressComponent> RunProgressComponent;

	// 캐릭터를 뒤쪽 위에서 따라가는 추적 카메라 암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 플레이어가 실제로 바라보는 추적 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<UCameraComponent> FollowCamera;

private:
	// Project Settings의 Go Deliver 설정값을 캐시하고 컴포넌트 기본값에 반영한다.
	void ApplyGameSettings();

	// 카운트다운 시작 시 현재 캐릭터 메시의 BowlSocketName에 바구니 Actor를 1회 부착한다.
	void AttachBowlForRun();

	// 런 실패/레벨 종료 시 런타임 바구니 Actor를 제거한다.
	void DetachAndDestroyBowl();

	// 러너 기준 전방 축으로 매 프레임 자동 전진 입력을 넣는다.
	void UpdateForwardMovement();

	// 시작 높이 기준 추락 실패 조건을 확인하고 GameMode 실패 흐름으로 전달한다.
	void CheckFallFailure();

	// 직선 주행 중 자동으로 발생하는 좌우 중심 흔들림을 갱신한다.
	void UpdateBaseInstability(float DeltaSeconds);

	// 착지 직후 완화를 적용한 플레이어 중심 입력값을 반환한다. 입력 이벤트가 없는 프레임에도 완화가 적용·해제되도록 매 프레임 평가한다.
	float GetLandingScaledBalanceInput() const;

	// 기본 흔들림, 플레이어 입력, 환경 효과를 합산한 최종 중심 입력값을 반환한다.
	float GetEffectiveBalanceInput() const;

	// 적재물 물리 흔들림에 사용할 중심 입력값을 반환합니다. 눈바람 중에는 플레이어 입력 기여분만 추가 증폭합니다.
	float GetCargoBalanceInputForPhysics() const;

	// 실제 점프가 시작된 순간 캐릭터 성별에 맞는 점프 사운드를 재생합니다.
	void PlayJumpSound();

	// 중심 입력값을 캐릭터 메시의 좌우 기울기 연출로 변환한다.
	void UpdateBodyLean(float DeltaSeconds);

	// BeginPlay에서 캐시한 자동 전진/몸 기울기 설정
	FPGRunnerSettings CachedRunnerSettings;

	// BeginPlay에서 캐시한 기본 중심 불안정성 설정
	FPGBalanceSettings CachedBalanceSettings;

	// BeginPlay에서 캐시한 점프 설정
	FPGJumpSettings CachedJumpSettings;

	// BeginPlay에서 캐시한 터치 드래그/플릭 조작 설정
	FPGTouchControlSettings CachedTouchControlSettings;

	// BeginPlay에서 캐시한 카메라 설정
	FPGCameraSettings CachedCameraSettings;

	// BeginPlay에서 캐시한 물리 적재물 설정
	FPGCargoSettings CachedCargoSettings;

	// BeginPlay에서 캐시한 바람/눈바람 환경 이벤트 설정
	FPGEnvironmentSettings CachedEnvironmentSettings;

	// 현재 플레이어가 입력 중인 좌우 중심 보정값
	float BalanceInput = 0.0f;

	// 직선 주행 중 자동으로 발생하는 기본 좌우 중심 흔들림 입력값
	float BaseInstabilityInput = 0.0f;

	// 좌우 플릭 직후 짧게 더하는 보조 중심 입력값. 이동 가속, 적재물 힘, 몸 기울기에 함께 반영된다.
	float FlickBalanceInput = 0.0f;

	// 좌우 플릭 보조 입력이 남아 있는 시간
	float FlickBalanceInputRemainingTime = 0.0f;

	// 기본 흔들림 위상 계산에 사용하는 런 진행 시간
	float BalanceElapsedTime = 0.0f;

	// 현재 러너가 조작 가능한 런 진행 상태인지 여부
	bool bIsRunActive = false;

	// 카운트다운 동안 플레이어 입력은 막고 자동 전진만 유지하는 상태입니다.
	bool bIsRunCountdownActive = false;

	// Home, Countdown, Failed 상태에서 점프와 중심 보정 입력을 무시하기 위한 잠금입니다.
	bool bIsGameplayInputLocked = true;

	// 추락 실패 판정 기준이 되는 시작 월드 Z 위치
	float StartLocationZ = 0.0f;

	// 런 진행 거리 계산 기준이 되는 시작 월드 위치
	FVector RunStartLocation = FVector::ZeroVector;

	// 런 진행 거리 계산에 사용하는 시작 시점 전방 방향
	FVector RunStartForwardVector = FVector::ForwardVector;

	// 런 경과 시간 계산에 사용하는 월드 시간
	float RunStartTimeSeconds = 0.0f;

	// Home/Shop 프리뷰에 사용할 에디터 배치 기준 Transform
	FTransform HomePreviewTransform = FTransform::Identity;

	bool bHasHomePreviewTransform = false;

	// 현재 적용된 캐릭터 스킨 데이터. 캐릭터 성별 기반 점프 사운드 선택에 사용합니다.
	UPROPERTY(Transient)
	TObjectPtr<UPGCharacterSkinDefinition> CurrentCharacterSkinDefinition;

	// 메시 연출에 적용 중인 현재 롤 각도
	float CurrentBodyRoll = 0.0f;

	// 몸 기울기 연출을 더하기 위한 Skeletal Mesh 초기 상대 회전
	FRotator InitialMeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<AActor> AttachedBowlActor;
};
