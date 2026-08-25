// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * SaveGame payload를 세대 번호와 CRC가 포함된 ProjectG 저장 envelope로 변환하는 내부 codec입니다.
 * 슬롯 선택과 UObject 직렬화는 UPGUserDataSubsystem이 담당하고, 이 경계는 기존 wire format만 보존합니다.
 */
namespace PGUserSaveCodec
{
	/** 직렬화된 SaveGame payload를 저장 envelope로 감쌉니다. 빈 payload는 기록하지 않습니다. */
	bool Encode(const TArray<uint8>& Payload, uint64 Generation, TArray<uint8>& OutSaveData);

	/** 저장 envelope를 검증하고 payload와 세대 번호를 복원합니다. 손상된 데이터는 false를 반환합니다. */
	bool Decode(const TArray<uint8>& SaveData, TArray<uint8>& OutPayload, uint64& OutGeneration);
}
