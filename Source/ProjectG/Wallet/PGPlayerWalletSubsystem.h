// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PGPlayerWalletSubsystem.generated.h"

class UPGPlayerWalletSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPGReceiptBalanceChangedSignature, int32, NewBalance, FName, Reason);

/**
 * 앱 실행 동안 플레이어의 누적 재화를 관리하고 SaveGame에 저장하는 지갑 서브시스템입니다.
 * 런 중 획득량은 RunProgress가 관리하고, 런 종료 정산/상점 구매만 이 지갑에 반영합니다.
 */
UCLASS()
class PROJECTG_API UPGPlayerWalletSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 현재 저장된 누적 영수증 보유량입니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Wallet")
	int32 GetReceiptBalance() const;

	// 영수증을 누적 재화에 더하고 즉시 저장합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Wallet")
	void AddReceipts(int32 Amount, FName Reason);

	// 현재 보유량으로 지정한 영수증을 사용할 수 있는지 확인합니다.
	UFUNCTION(BlueprintPure, Category = "ProjectG | Wallet")
	bool CanSpendReceipts(int32 Amount) const;

	// 상점 구매 등에서 영수증을 차감합니다. 잔액 부족 시 false를 반환합니다.
	UFUNCTION(BlueprintCallable, Category = "ProjectG | Wallet")
	bool TrySpendReceipts(int32 Amount, FName Reason);

	// 누적 영수증 보유량이 바뀌면 Home/Shop UI가 갱신할 수 있도록 알립니다.
	UPROPERTY(BlueprintAssignable, Category = "ProjectG | Wallet")
	FPGReceiptBalanceChangedSignature OnReceiptBalanceChanged;

private:
	void LoadWallet();
	bool SaveWallet() const;
	UPGPlayerWalletSaveGame* EnsureWallet();

	static const TCHAR* WalletSaveSlotName;
	static constexpr int32 WalletUserIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPGPlayerWalletSaveGame> WalletSaveGame;
};
