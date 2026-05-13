// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wallet/PGPlayerWalletSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Wallet/PGPlayerWalletSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGPlayerWallet, Log, All);

const TCHAR* UPGPlayerWalletSubsystem::WalletSaveSlotName = TEXT("PGPlayerWallet");

void UPGPlayerWalletSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadWallet();
}

void UPGPlayerWalletSubsystem::Deinitialize()
{
	SaveWallet();
	WalletSaveGame = nullptr;
	Super::Deinitialize();
}

int32 UPGPlayerWalletSubsystem::GetReceiptBalance() const
{
	return WalletSaveGame ? FMath::Max(0, WalletSaveGame->ReceiptBalance) : 0;
}

void UPGPlayerWalletSubsystem::AddReceipts(int32 Amount, FName Reason)
{
	if (Amount <= 0)
	{
		return;
	}

	UPGPlayerWalletSaveGame* Wallet = EnsureWallet();
	if (!Wallet)
	{
		return;
	}

	Wallet->ReceiptBalance = FMath::Max(0, Wallet->ReceiptBalance) + Amount;
	SaveWallet();
	OnReceiptBalanceChanged.Broadcast(Wallet->ReceiptBalance, Reason);
}

bool UPGPlayerWalletSubsystem::CanSpendReceipts(int32 Amount) const
{
	return Amount > 0 && GetReceiptBalance() >= Amount;
}

bool UPGPlayerWalletSubsystem::TrySpendReceipts(int32 Amount, FName Reason)
{
	if (!CanSpendReceipts(Amount))
	{
		return false;
	}

	UPGPlayerWalletSaveGame* Wallet = EnsureWallet();
	if (!Wallet)
	{
		return false;
	}

	Wallet->ReceiptBalance = FMath::Max(0, Wallet->ReceiptBalance - Amount);
	SaveWallet();
	OnReceiptBalanceChanged.Broadcast(Wallet->ReceiptBalance, Reason);
	return true;
}

void UPGPlayerWalletSubsystem::LoadWallet()
{
	if (USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(WalletSaveSlotName, WalletUserIndex))
	{
		WalletSaveGame = Cast<UPGPlayerWalletSaveGame>(LoadedSave);
	}

	if (!WalletSaveGame)
	{
		WalletSaveGame = Cast<UPGPlayerWalletSaveGame>(UGameplayStatics::CreateSaveGameObject(UPGPlayerWalletSaveGame::StaticClass()));
	}

	if (WalletSaveGame)
	{
		WalletSaveGame->ReceiptBalance = FMath::Max(0, WalletSaveGame->ReceiptBalance);
	}
}

bool UPGPlayerWalletSubsystem::SaveWallet() const
{
	if (!WalletSaveGame)
	{
		return false;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(WalletSaveGame, WalletSaveSlotName, WalletUserIndex);
	if (!bSaved)
	{
		UE_LOG(LogPGPlayerWallet, Warning, TEXT("Failed to save player wallet to slot '%s'."), WalletSaveSlotName);
	}
	return bSaved;
}

UPGPlayerWalletSaveGame* UPGPlayerWalletSubsystem::EnsureWallet()
{
	if (!WalletSaveGame)
	{
		LoadWallet();
	}
	return WalletSaveGame;
}
