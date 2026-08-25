// Copyright Epic Games, Inc. All Rights Reserved.

#include "User/PGUserSaveCodec.h"

#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
constexpr uint32 UserSaveEnvelopeMagic = 0x50475553; // PGUS
constexpr uint32 UserSaveEnvelopeVersion = 1;
constexpr int32 UserSaveEnvelopeSize = sizeof(uint32) * 4 + sizeof(uint64);

/** 디스크에 기록되는 필드 순서를 고정하는 저장 envelope 헤더입니다. */
struct FPGUserSaveEnvelope
{
	uint32 Magic = UserSaveEnvelopeMagic;
	uint32 Version = UserSaveEnvelopeVersion;
	uint64 Generation = 0;
	uint32 PayloadSize = 0;
	uint32 PayloadCrc = 0;

	friend FArchive& operator<<(FArchive& Archive, FPGUserSaveEnvelope& Envelope)
	{
		Archive << Envelope.Magic;
		Archive << Envelope.Version;
		Archive << Envelope.Generation;
		Archive << Envelope.PayloadSize;
		Archive << Envelope.PayloadCrc;
		return Archive;
	}
};
}

bool PGUserSaveCodec::Encode(const TArray<uint8>& Payload, uint64 Generation, TArray<uint8>& OutSaveData)
{
	OutSaveData.Reset();
	if (Payload.IsEmpty())
	{
		return false;
	}

	FPGUserSaveEnvelope Envelope;
	Envelope.Generation = Generation;
	Envelope.PayloadSize = Payload.Num();
	Envelope.PayloadCrc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());

	FMemoryWriter Writer(OutSaveData, true);
	Writer << Envelope;
	Writer.Serialize(const_cast<uint8*>(Payload.GetData()), Payload.Num());
	return !Writer.IsError();
}

bool PGUserSaveCodec::Decode(const TArray<uint8>& SaveData, TArray<uint8>& OutPayload, uint64& OutGeneration)
{
	OutPayload.Reset();
	OutGeneration = 0;
	if (SaveData.Num() < UserSaveEnvelopeSize)
	{
		return false;
	}

	FMemoryReader Reader(SaveData, true);
	FPGUserSaveEnvelope Envelope;
	Reader << Envelope;
	if (Reader.IsError()
		|| Envelope.Magic != UserSaveEnvelopeMagic
		|| Envelope.Version != UserSaveEnvelopeVersion
		|| Envelope.PayloadSize == 0
		|| Envelope.PayloadSize != static_cast<uint32>(SaveData.Num() - UserSaveEnvelopeSize))
	{
		return false;
	}

	OutPayload.SetNumUninitialized(Envelope.PayloadSize);
	Reader.Serialize(OutPayload.GetData(), Envelope.PayloadSize);
	if (Reader.IsError() || FCrc::MemCrc32(OutPayload.GetData(), OutPayload.Num()) != Envelope.PayloadCrc)
	{
		OutPayload.Reset();
		return false;
	}

	OutGeneration = Envelope.Generation;
	return true;
}
