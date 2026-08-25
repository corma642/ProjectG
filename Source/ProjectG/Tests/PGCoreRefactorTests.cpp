// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
