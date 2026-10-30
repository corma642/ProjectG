// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/PGRunnerCharacter.h"
#include "Misc/AutomationTest.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "UI/PGUIAnimationUtils.h"
#include "User/PGUserSaveCodec.h"
#include "Utilities/PGWeightedRandom.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGWeightedRandomBoundaryTest,
	"ProjectG.Core.WeightedRandom.Boundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGWeightedRandomBoundaryTest::RunTest(const FString& Parameters)
{
	const TArray<float> EmptyWeights;
	TestEqual(
		TEXT("빈 후보는 INDEX_NONE을 반환해야 합니다."),
		PGWeightedRandom::SelectIndexByFraction(EmptyWeights.Num(), [&EmptyWeights](int32 Index) { return EmptyWeights[Index]; }, 0.0f),
		INDEX_NONE);

	const TArray<float> NonPositiveWeights = {0.0f, -1.0f, 0.0f};
	TestEqual(
		TEXT("0 이하 가중치만 있으면 INDEX_NONE을 반환해야 합니다."),
		PGWeightedRandom::SelectIndexByFraction(NonPositiveWeights.Num(), [&NonPositiveWeights](int32 Index) { return NonPositiveWeights[Index]; }, 0.5f),
		INDEX_NONE);

	const TArray<float> BoundaryWeights = {0.0f, 1.0f};
	TestEqual(
		TEXT("난수 경계값 0에서도 가중치 0인 첫 후보를 선택하면 안 됩니다."),
		PGWeightedRandom::SelectIndexByFraction(BoundaryWeights.Num(), [&BoundaryWeights](int32 Index) { return BoundaryWeights[Index]; }, 0.0f),
		1);

	const TArray<float> SingleEligibleWeight = {-4.0f, 0.0f, 2.0f, 0.0f};
	TestEqual(
		TEXT("유효 후보가 하나면 fraction과 무관하게 해당 후보를 선택해야 합니다."),
		PGWeightedRandom::SelectIndexByFraction(SingleEligibleWeight.Num(), [&SingleEligibleWeight](int32 Index) { return SingleEligibleWeight[Index]; }, 0.999999f),
		2);

	const TArray<float> SmallPositiveWeight = {0.0f, UE_SMALL_NUMBER};
	TestEqual(
		TEXT("0보다 큰 미세 가중치도 유효 후보로 처리해야 합니다."),
		PGWeightedRandom::SelectIndexByFraction(SmallPositiveWeight.Num(), [&SmallPositiveWeight](int32 Index) { return SmallPositiveWeight[Index]; }, 0.0f),
		1);

	const TArray<float> ReproducibleWeights = {1.0f, 2.0f, 3.0f};
	FRandomStream FirstStream(1337);
	FRandomStream SecondStream(1337);
	for (int32 Iteration = 0; Iteration < 64; ++Iteration)
	{
		const int32 FirstIndex = PGWeightedRandom::SelectIndex(ReproducibleWeights.Num(), [&ReproducibleWeights](int32 Index) { return ReproducibleWeights[Index]; }, FirstStream);
		const int32 SecondIndex = PGWeightedRandom::SelectIndex(ReproducibleWeights.Num(), [&ReproducibleWeights](int32 Index) { return ReproducibleWeights[Index]; }, SecondStream);
		TestEqual(TEXT("같은 seed의 선택 순서는 재현 가능해야 합니다."), FirstIndex, SecondIndex);
		TestTrue(TEXT("선택 결과는 후보 배열의 유효 인덱스여야 합니다."), ReproducibleWeights.IsValidIndex(FirstIndex));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGUserSaveCodecTest,
	"ProjectG.Core.UserSaveCodec.EnvelopeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGUserSaveCodecTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Payload = {0x10, 0x20, 0x30, 0x40, 0x50};
	constexpr uint64 Generation = 42;
	TArray<uint8> EncodedData;
	TestTrue(TEXT("유효 payload는 저장 envelope로 인코딩되어야 합니다."), PGUserSaveCodec::Encode(Payload, Generation, EncodedData));

	TArray<uint8> DecodedPayload;
	uint64 DecodedGeneration = 0;
	TestTrue(TEXT("정상 envelope는 디코딩되어야 합니다."), PGUserSaveCodec::Decode(EncodedData, DecodedPayload, DecodedGeneration));
	TestTrue(TEXT("디코딩한 payload가 원본과 같아야 합니다."), DecodedPayload == Payload);
	TestEqual(TEXT("저장 세대 번호를 보존해야 합니다."), DecodedGeneration, Generation);

	TArray<uint8> ShortData = EncodedData;
	ShortData.SetNum(8);
	TestFalse(TEXT("헤더보다 짧은 데이터는 거부해야 합니다."), PGUserSaveCodec::Decode(ShortData, DecodedPayload, DecodedGeneration));

	TArray<uint8> InvalidMagic = EncodedData;
	InvalidMagic[0] ^= 0x01;
	TestFalse(TEXT("magic이 다른 envelope는 거부해야 합니다."), PGUserSaveCodec::Decode(InvalidMagic, DecodedPayload, DecodedGeneration));

	TArray<uint8> InvalidVersion = EncodedData;
	InvalidVersion[sizeof(uint32)] ^= 0x01;
	TestFalse(TEXT("지원하지 않는 version은 거부해야 합니다."), PGUserSaveCodec::Decode(InvalidVersion, DecodedPayload, DecodedGeneration));

	TArray<uint8> InvalidSize = EncodedData;
	constexpr int32 PayloadSizeOffset = sizeof(uint32) * 2 + sizeof(uint64);
	InvalidSize[PayloadSizeOffset] ^= 0x01;
	TestFalse(TEXT("payload 크기가 실제 데이터와 다르면 거부해야 합니다."), PGUserSaveCodec::Decode(InvalidSize, DecodedPayload, DecodedGeneration));

	TArray<uint8> InvalidCrc = EncodedData;
	InvalidCrc.Last() ^= 0x01;
	TestFalse(TEXT("payload CRC가 다르면 손상된 저장으로 거부해야 합니다."), PGUserSaveCodec::Decode(InvalidCrc, DecodedPayload, DecodedGeneration));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGUIAnimationEasingTest,
	"ProjectG.Core.UIAnimation.EasingCurves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGUIAnimationEasingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("EaseInCubic 시작값"), PGUIAnimation::EaseInCubic(0.0f), 0.0f);
	TestTrue(TEXT("EaseInCubic 중간값"), FMath::IsNearlyEqual(PGUIAnimation::EaseInCubic(0.5f), 0.125f));
	TestEqual(TEXT("EaseInCubic 종료값"), PGUIAnimation::EaseInCubic(1.0f), 1.0f);

	TestEqual(TEXT("EaseOutCubic 시작값"), PGUIAnimation::EaseOutCubic(0.0f), 0.0f);
	TestTrue(TEXT("EaseOutCubic 중간값"), FMath::IsNearlyEqual(PGUIAnimation::EaseOutCubic(0.5f), 0.875f));
	TestEqual(TEXT("EaseOutCubic 종료값"), PGUIAnimation::EaseOutCubic(1.0f), 1.0f);

	TestTrue(TEXT("EaseInOutCubic 중앙값"), FMath::IsNearlyEqual(PGUIAnimation::EaseInOutCubic(0.5f), 0.5f));
	TestEqual(TEXT("EaseInOutCubic 시작값"), PGUIAnimation::EaseInOutCubic(0.0f), 0.0f);
	TestEqual(TEXT("EaseInOutCubic 종료값"), PGUIAnimation::EaseInOutCubic(1.0f), 1.0f);
	TestEqual(TEXT("EaseOutBack 시작값"), PGUIAnimation::EaseOutBack(0.0f), 0.0f);
	TestTrue(TEXT("기본 back overshoot를 보존해야 합니다."), FMath::IsNearlyEqual(PGUIAnimation::EaseOutBack(0.5f), 1.0876975f));
	TestTrue(TEXT("Receipt Shop의 완화된 overshoot를 보존해야 합니다."), FMath::IsNearlyEqual(PGUIAnimation::EaseOutBack(0.5f, 1.45f), 1.05625f));
	TestEqual(TEXT("EaseOutBack 종료값"), PGUIAnimation::EaseOutBack(1.0f), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGTutorialRewindSnapshotTest,
	"ProjectG.Core.Tutorial.RewindSnapshotSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGTutorialRewindSnapshotTest::RunTest(const FString& Parameters)
{
	TArray<FPGRunRewindSnapshot> Snapshots;
	for (const float Time : {10.0f, 10.5f, 11.0f, 11.5f, 12.0f})
	{
		Snapshots.AddDefaulted_GetRef().WorldTimeSeconds = Time;
	}

	TestEqual(TEXT("빈 목록은 INDEX_NONE이어야 합니다."), APGRunnerCharacter::SelectRewindSnapshotIndex(TArrayView<const FPGRunRewindSnapshot>(), 15.0f, 3.0f), INDEX_NONE);
	TestEqual(TEXT("목표 시각(12.2) 이전의 가장 최근 스냅샷(12.0)을 골라야 합니다."), APGRunnerCharacter::SelectRewindSnapshotIndex(Snapshots, 15.2f, 3.0f), 4);
	TestEqual(TEXT("목표 시각(10.6) 이전의 가장 최근 스냅샷(10.5)을 골라야 합니다."), APGRunnerCharacter::SelectRewindSnapshotIndex(Snapshots, 13.6f, 3.0f), 1);
	TestEqual(TEXT("목표 시각과 같은 시각의 스냅샷도 과거로 봐야 합니다."), APGRunnerCharacter::SelectRewindSnapshotIndex(Snapshots, 14.0f, 3.0f), 2);
	TestEqual(TEXT("모든 스냅샷이 목표 시각 뒤면 가장 오래된 것으로 가야 합니다."), APGRunnerCharacter::SelectRewindSnapshotIndex(Snapshots, 11.0f, 3.0f), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGBalanceImpulseSpeedLimitTest,
	"ProjectG.Core.BalanceMovement.ImpulseSpeedLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGBalanceImpulseSpeedLimitTest::RunTest(const FString& Parameters)
{
	constexpr float NormalMaxSpeed = 100.0f;
	constexpr float DampingSpeed = 4.0f;
	constexpr float DeltaSeconds = 1.0f / 60.0f;

	float ImpulseSpeedLimit = 0.0f;
	TestEqual(
		TEXT("충격이 없으면 일반 상한으로 제한해야 합니다."),
		UPGRunnerMovementComponent::ClampBalanceVelocity(250.0f, NormalMaxSpeed, DampingSpeed, DeltaSeconds, ImpulseSpeedLimit),
		NormalMaxSpeed);
	TestEqual(TEXT("충격이 없으면 허용 상한이 생기지 않아야 합니다."), ImpulseSpeedLimit, 0.0f);

	ImpulseSpeedLimit = 600.0f;
	float Velocity = UPGRunnerMovementComponent::ClampBalanceVelocity(600.0f, NormalMaxSpeed, DampingSpeed, DeltaSeconds, ImpulseSpeedLimit);
	TestTrue(TEXT("충격 속도는 일반 상한으로 즉시 잘리면 안 됩니다."), Velocity > NormalMaxSpeed);
	TestTrue(TEXT("충격 속도는 첫 Tick부터 감쇠로 줄어야 합니다."), Velocity < 600.0f);

	// 밀린 방향 입력이 매 Tick 속도를 다시 끌어올려도 허용 상한은 감쇠로 줄어 일반 상한으로 돌아와야 합니다.
	int32 StepCount = 0;
	while (ImpulseSpeedLimit > 0.0f && StepCount < 600)
	{
		const float PreviousVelocity = Velocity;
		Velocity = UPGRunnerMovementComponent::ClampBalanceVelocity(Velocity + 100.0f, NormalMaxSpeed, DampingSpeed, DeltaSeconds, ImpulseSpeedLimit);
		TestTrue(TEXT("입력 가속으로 초과 속도가 커지거나 유지되면 안 됩니다."), Velocity < PreviousVelocity);
		++StepCount;
	}
	TestTrue(TEXT("초과 속도는 유한한 시간 안에 해제되어야 합니다."), StepCount < 600);
	TestEqual(TEXT("허용이 해제되면 일반 상한으로 돌아와야 합니다."), Velocity, NormalMaxSpeed);

	ImpulseSpeedLimit = 600.0f;
	TestEqual(
		TEXT("감쇠가 0이면 허용 상한을 줄이지 않아야 합니다."),
		UPGRunnerMovementComponent::ClampBalanceVelocity(600.0f, NormalMaxSpeed, 0.0f, DeltaSeconds, ImpulseSpeedLimit),
		600.0f);
	TestEqual(TEXT("감쇠가 0이면 허용 상한이 그대로 남아야 합니다."), ImpulseSpeedLimit, 600.0f);

	ImpulseSpeedLimit = 600.0f;
	const float NegativeVelocity = UPGRunnerMovementComponent::ClampBalanceVelocity(-600.0f, NormalMaxSpeed, DampingSpeed, DeltaSeconds, ImpulseSpeedLimit);
	TestTrue(TEXT("왼쪽 방향 충격도 같은 규칙으로 허용하고 감쇠해야 합니다."), NegativeVelocity > -600.0f && NegativeVelocity < -NormalMaxSpeed);

	// 설정 상한이 일반 상한 이하면 큰 충격량도 그 Tick 상태의 일반 상한까지만 적용되어야 합니다.
	constexpr float AirborneMaxSpeed = 200.0f;
	constexpr float AirborneDampingSpeed = 0.8f;
	ImpulseSpeedLimit = 100.0f;
	TestEqual(
		TEXT("공중에서는 설정 상한이 낮아도 공중 일반 상한까지 밀려야 합니다."),
		UPGRunnerMovementComponent::ClampBalanceVelocity(2000.0f, AirborneMaxSpeed, AirborneDampingSpeed, DeltaSeconds, ImpulseSpeedLimit),
		AirborneMaxSpeed);
	TestEqual(TEXT("일반 상한 안의 속도에는 허용 상한이 남지 않아야 합니다."), ImpulseSpeedLimit, 0.0f);

	ImpulseSpeedLimit = 0.0f;
	TestEqual(
		TEXT("허용 상한이 0이면 큰 충격량도 일반 상한까지만 적용되어야 합니다."),
		UPGRunnerMovementComponent::ClampBalanceVelocity(2000.0f, NormalMaxSpeed, DampingSpeed, DeltaSeconds, ImpulseSpeedLimit),
		NormalMaxSpeed);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
