// Fill out your copyright notice in the Description page of Project Settings.


#include "Settings/PGGameSettings.h"

UPGGameSettings::UPGGameSettings()
{
	CategoryName = TEXT("ProjectG");
	SectionName = TEXT("GoDeliver");

	MobileQuality.Low.ResolutionScalePercent = 70.0f;
	MobileQuality.Low.AntiAliasingQuality = 0;
	MobileQuality.Low.TextureQuality = 0;
	MobileQuality.Low.ShadowQuality = 0;
	MobileQuality.Low.PostProcessQuality = 0;
	MobileQuality.Low.EffectsQuality = 0;
	MobileQuality.Low.ViewDistanceQuality = 0;
	MobileQuality.Low.MaxAnisotropy = 1;

	MobileQuality.Balanced.ResolutionScalePercent = 90.0f;
	MobileQuality.Balanced.AntiAliasingQuality = 2;
	MobileQuality.Balanced.TextureQuality = 2;
	MobileQuality.Balanced.ShadowQuality = 1;
	MobileQuality.Balanced.PostProcessQuality = 1;
	MobileQuality.Balanced.EffectsQuality = 1;
	MobileQuality.Balanced.ViewDistanceQuality = 2;
	MobileQuality.Balanced.MaxAnisotropy = 4;

	MobileQuality.High.ResolutionScalePercent = 100.0f;
	MobileQuality.High.AntiAliasingQuality = 3;
	MobileQuality.High.TextureQuality = 3;
	MobileQuality.High.ShadowQuality = 3;
	MobileQuality.High.PostProcessQuality = 3;
	MobileQuality.High.EffectsQuality = 3;
	MobileQuality.High.ViewDistanceQuality = 3;
	MobileQuality.High.MaxAnisotropy = 8;

	MobileQuality.DefaultPreset = EPGMobileQualityPreset::High;

	if (Shop.ReceiptPacks.Num() == 0)
	{
		auto AddReceiptPack = [this](const FText& PackName, int32 ReceiptAmount, const TCHAR* PriceText)
		{
			FPGReceiptShopPack Pack;
			Pack.PackName = PackName;
			Pack.ReceiptAmount = ReceiptAmount;
			Pack.PriceText = PriceText;
			Shop.ReceiptPacks.Add(Pack);
		};

		AddReceiptPack(NSLOCTEXT("PGReceiptShop", "PackSmall", "스몰 팩"), 50, TEXT("$0.99"));
		AddReceiptPack(NSLOCTEXT("PGReceiptShop", "PackStandard", "스탠다드 팩"), 120, TEXT("$2.99"));
		AddReceiptPack(NSLOCTEXT("PGReceiptShop", "PackValue", "밸류 팩"), 260, TEXT("$5.99"));
		AddReceiptPack(NSLOCTEXT("PGReceiptShop", "PackMega", "메가 팩"), 700, TEXT("$12.99"));
	}
}
