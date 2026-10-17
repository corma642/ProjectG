// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Player/PGTouchGesture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPGTouchGestureClassifyFlickTest,
	"ProjectG.Core.TouchGesture.ClassifyFlick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPGTouchGestureClassifyFlickTest::RunTest(const FString& Parameters)
{
	using namespace PGTouchGesture;

	// 기준값은 픽셀로 환산된 뒤의 값이다: 최소 80px, 최대 0.2초, 최소 600px/s, 지배 비율 1.35.
	const auto Classify = [](const FVector2D& DeltaPx, float DurationSeconds, bool bAllowLateral)
	{
		return static_cast<int32>(ClassifyFlick(DeltaPx, DurationSeconds, 80.0f, 0.2f, 600.0f, 1.35f, bAllowLateral));
	};
	const int32 None = static_cast<int32>(EFlickDirection::None);

	TestEqual(TEXT("위로 100px를 0.1초에 튕기면 위 플릭"), Classify(FVector2D(0.0f, -100.0f), 0.1f, true), static_cast<int32>(EFlickDirection::Up));
	TestEqual(TEXT("드래그 중(좌우 불허)에도 위 플릭은 인정"), Classify(FVector2D(10.0f, -100.0f), 0.1f, false), static_cast<int32>(EFlickDirection::Up));
	TestEqual(TEXT("0.5초 느린 이동은 플릭이 아니다"), Classify(FVector2D(0.0f, -100.0f), 0.5f, true), None);
	TestEqual(TEXT("40px 짧은 이동은 플릭이 아니다"), Classify(FVector2D(0.0f, -40.0f), 0.05f, true), None);
	TestEqual(TEXT("80px를 0.2초(400px/s)는 최소 속도 미달"), Classify(FVector2D(0.0f, -80.0f), 0.2f, true), None);
	TestEqual(TEXT("오른쪽 플릭"), Classify(FVector2D(120.0f, 10.0f), 0.1f, true), static_cast<int32>(EFlickDirection::Right));
	TestEqual(TEXT("왼쪽 플릭"), Classify(FVector2D(-120.0f, 10.0f), 0.1f, true), static_cast<int32>(EFlickDirection::Left));
	TestEqual(TEXT("드래그 중에는 좌우 플릭을 받지 않는다"), Classify(FVector2D(120.0f, 10.0f), 0.1f, false), None);
	TestEqual(TEXT("대각선은 지배 비율 미달이라 플릭이 아니다"), Classify(FVector2D(80.0f, -80.0f), 0.1f, true), None);
	TestEqual(TEXT("아래 방향 플릭은 없다"), Classify(FVector2D(0.0f, 100.0f), 0.1f, true), None);
	return true;
}

#endif
