// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 후보 배열을 별도로 만들지 않고 가중치 기반 인덱스를 선택하는 내부 유틸리티입니다.
 * WeightResolver는 같은 인덱스에 대해 두 번 호출되므로 호출 사이에 상태를 바꾸지 않아야 합니다.
 */
namespace PGWeightedRandom
{
	/**
	 * [0, 1] 범위의 정규화된 난수로 후보를 선택합니다.
	 * 0 이하 가중치는 선택 대상에서 제외하며, 유효 후보가 없으면 INDEX_NONE을 반환합니다.
	 */
	template <typename WeightResolverType>
	int32 SelectIndexByFraction(int32 NumCandidates, WeightResolverType&& WeightResolver, float Fraction)
	{
		if (NumCandidates <= 0)
		{
			return INDEX_NONE;
		}

		float TotalWeight = 0.0f;
		for (int32 CandidateIndex = 0; CandidateIndex < NumCandidates; ++CandidateIndex)
		{
			const float Weight = WeightResolver(CandidateIndex);
			if (Weight > 0.0f)
			{
				TotalWeight += Weight;
			}
		}

		if (TotalWeight <= 0.0f)
		{
			return INDEX_NONE;
		}

		// FRandomStream::GetFraction은 [0, 1) 범위지만, 테스트나 외부 호출이 1을 넘겨도
		// 마지막 유효 후보로 안전하게 수렴하도록 입력을 방어적으로 제한합니다.
		const float Roll = FMath::Clamp(Fraction, 0.0f, 1.0f) * TotalWeight;
		float AccumulatedWeight = 0.0f;
		int32 LastEligibleIndex = INDEX_NONE;

		for (int32 CandidateIndex = 0; CandidateIndex < NumCandidates; ++CandidateIndex)
		{
			const float Weight = WeightResolver(CandidateIndex);
			if (Weight <= 0.0f)
			{
				continue;
			}

			LastEligibleIndex = CandidateIndex;
			AccumulatedWeight += Weight;
			if (Roll < AccumulatedWeight)
			{
				return CandidateIndex;
			}
		}

		// 누적 부동소수점 오차 또는 Fraction == 1인 방어 입력만 이 경로에 도달합니다.
		return LastEligibleIndex;
	}

	/** FRandomStream의 [0, 1) 난수를 사용해 가중치 기반 후보 인덱스를 반환합니다. */
	template <typename WeightResolverType>
	int32 SelectIndex(int32 NumCandidates, WeightResolverType&& WeightResolver, FRandomStream& RandomStream)
	{
		return SelectIndexByFraction(NumCandidates, Forward<WeightResolverType>(WeightResolver), RandomStream.GetFraction());
	}
}
