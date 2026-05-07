// Fill out your copyright notice in the Description page of Project Settings.


#include "Settings/PGGameSettings.h"

UPGGameSettings::UPGGameSettings()
{
	CategoryName = TEXT("ProjectG");
	SectionName = TEXT("GoDeliver");

	MobileQuality.Low.ResolutionScalePercent = 70.0f;
	MobileQuality.Low.FrameRateLimit = 30;
	MobileQuality.Low.MobileAntiAliasingMode = 1;
	MobileQuality.Low.AntiAliasingQuality = 0;
	MobileQuality.Low.TextureQuality = 0;
	MobileQuality.Low.ShadowQuality = 0;
	MobileQuality.Low.PostProcessQuality = 0;
	MobileQuality.Low.EffectsQuality = 0;
	MobileQuality.Low.ViewDistanceQuality = 0;
	MobileQuality.Low.MaxAnisotropy = 1;

	MobileQuality.Balanced.ResolutionScalePercent = 90.0f;
	MobileQuality.Balanced.FrameRateLimit = 60;
	MobileQuality.Balanced.MobileAntiAliasingMode = 2;
	MobileQuality.Balanced.AntiAliasingQuality = 2;
	MobileQuality.Balanced.TextureQuality = 2;
	MobileQuality.Balanced.ShadowQuality = 1;
	MobileQuality.Balanced.PostProcessQuality = 1;
	MobileQuality.Balanced.EffectsQuality = 1;
	MobileQuality.Balanced.ViewDistanceQuality = 2;
	MobileQuality.Balanced.MaxAnisotropy = 4;

	MobileQuality.High.ResolutionScalePercent = 100.0f;
	MobileQuality.High.FrameRateLimit = 60;
	MobileQuality.High.MobileAntiAliasingMode = 2;
	MobileQuality.High.AntiAliasingQuality = 3;
	MobileQuality.High.TextureQuality = 3;
	MobileQuality.High.ShadowQuality = 3;
	MobileQuality.High.PostProcessQuality = 3;
	MobileQuality.High.EffectsQuality = 3;
	MobileQuality.High.ViewDistanceQuality = 3;
	MobileQuality.High.MaxAnisotropy = 8;

	MobileQuality.Ultra.ResolutionScalePercent = 100.0f;
	MobileQuality.Ultra.FrameRateLimit = 60;
	MobileQuality.Ultra.MobileAntiAliasingMode = 2;
	MobileQuality.Ultra.AntiAliasingQuality = 4;
	MobileQuality.Ultra.TextureQuality = 4;
	MobileQuality.Ultra.ShadowQuality = 4;
	MobileQuality.Ultra.PostProcessQuality = 4;
	MobileQuality.Ultra.EffectsQuality = 4;
	MobileQuality.Ultra.ViewDistanceQuality = 4;
	MobileQuality.Ultra.MaxAnisotropy = 8;
	MobileQuality.DefaultPreset = EPGMobileQualityPreset::Ultra;
}
