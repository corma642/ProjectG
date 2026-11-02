// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Async/Async.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Cargo/PGCargoStackComponent.h"
#include "Cargo/PGPhysicsCargoActor.h"
#include "Character/PGRunnerCharacter.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/PGGameModeBase.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Movement/PGRunnerMovementComponent.h"
#include "PBDRigidsSolver.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Player/PGPlayerController.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Rendering/SlateRenderer.h"
#include "RunEvents/PGRunEventSubsystem.h"
#include "RunProgress/PGRunProgressComponent.h"
#include "Track/PGRunDirector.h"
#include "Track/PGTrackSegment.h"
#include "Track/PGTrackSegmentSet.h"
#include "UI/PGHomeScreenWidget.h"
#include "UI/PGRunResultWidget.h"
#include "UI/PGShopScreenWidget.h"
#include "UnrealClient.h"
#include "User/PGUserDataSubsystem.h"
#include "Widgets/SWindow.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogPGShowcase, Log, All);

/**
 * 개발 빌드 전용 쇼케이스 녹화 명령입니다. 작업 결과를 영상으로 확인할 수 있게 사람 없이 게임을 진행시키며 화면을 저장합니다.
 * Shipping 빌드에는 포함되지 않습니다. 실행과 인코딩은 Tools/Record-Showcase.ps1이 맡습니다.
 *
 * PG.Showcase.DumpFrames <출력 폴더> <FPS> <프레임 수>
 *   게임 화면(UI 포함)을 frame_00000.png부터 순서대로 저장하고, 다 저장하면 프로세스를 종료합니다. -game 실행에서만 동작합니다.
 *   덤프 중에는 게임 시간이 프레임마다 정확히 1/FPS초씩 흐르고 실제 시간도 그 속도에 맞춰집니다.
 * PG.Showcase.Play <스크립트 파일>
 *   한 줄에 "<초> <콘솔 명령>" 형식으로 적힌 명령을 그 시각에 실행합니다. '#'로 시작하는 줄은 주석입니다.
 *   시각은 Play를 호출한 뒤 흐른 게임 프레임 시간이며, 슬로모션의 영향을 받지 않습니다.
 * PG.Showcase.WaitForRun
 *   스크립트 안에서 사용합니다. 런이 시작될(GO) 때까지 스크립트 시계를 멈춰, 이후 명령의 시각이 예열 시간과 무관하게 런 시작 기준이 되게 합니다.
 * PG.Showcase.Background
 *   앱이 뒤로 가는 순간(ApplicationWillDeactivateDelegate)을 흉내 내 자동 일시정지를 연출합니다. Resume은 사람이 누르는 버튼이라 명령이 없습니다.
 * PG.Showcase.Back
 *   Android 뒤로가기를 흉내 냅니다(APGPlayerController::HandleBackRequested). 모달 닫기, Pause 토글, Home 종료 확인을 연출합니다.
 * PG.Showcase.Shop [Characters|Maps]
 *   Home의 Shop 버튼처럼 상점을 열고, 상점이 이미 열려 있으면 탭(기본 Characters)을 바꿉니다. 스토어 스크린샷용입니다.
 * PG.Showcase.Language <ko|en>
 *   게임 언어를 바꿉니다(저장하지 않음). 열려 있는 화면의 문구가 그 자리에서 바뀌는지 연출합니다.
 * PG.Showcase.Continue
 *   결과 화면의 이어하기 버튼과 같은 일을 합니다(APGGameModeBase::RequestContinueRun). 거절되면 Warning을 남겨 녹화가 실패로 처리됩니다.
 * PG.Showcase.ResultButton <BindWidget 이름>
 *   열려 있는 결과 화면의 버튼(ContinueButton·AdContinueButton·DoubleReceiptsButton·RetryButton·HomeButton)을 플레이어처럼 누릅니다.
 *   화면이 없거나 버튼이 꺼져 있으면 Warning을 남겨 녹화가 실패로 처리됩니다. 광고 버튼은 데스크톱에서 가짜 광고(FakeAdDurationSeconds) 뒤 보상을 줍니다.
 * PG.Showcase.Receipts <수>
 *   저장된 지갑에 영수증을 더합니다. 이 PC의 저장 데이터가 실제로 바뀌므로 이어하기 비용만큼만 더해 씁니다.
 * PG.Showcase.RunReceipts <수>
 *   달리는 중인 러너의 이번 런 영수증을 더합니다(영수증을 주운 것과 같음). 결과 화면의 영수증 2배 버튼은 런 영수증이 1장 이상일 때만 보입니다.
 * PG.Showcase.AutoPilot <0|1>
 *   입력 없이도 러너가 길을 따라가도록 좌우 이동과 점프를 대신 넣습니다.
 * PG.Showcase.Balance <입력 -1~1> [초]
 *   지정한 시간 동안(기본 0.5초) 러너 중심 입력을 강제합니다. 자동 조종보다 뒤에 적용되므로 첫 런 가이드처럼 입력에 반응하는 UI를 연출할 수 있습니다.
 * PG.Showcase.Jump
 *   플레이어 입력처럼 점프를 한 번 요청합니다. 공중이거나 쿨타임이면 거절되고 Display 로그만 남깁니다.
 * PG.Showcase.FlickJump <-1|1>
 *   좌우 플릭 점프를 한 번 요청합니다(점프 + 짧은 측면 이동). 튜토리얼 좌우 이동 단계를 연출합니다.
 * PG.Showcase.RunEvent <Wind|Snowstorm|MovingHazard> [방향 -1~1]
 *   지정한 런 이벤트를 지금 전조부터 시작합니다. MovingHazard의 방향은 -1 왼쪽, 0 중앙, 1 오른쪽 구역이고, Wind는 음수면 왼쪽 그 외에는 오른쪽입니다.
 * PG.Showcase.CargoImpulse <속도 cm/s>
 *   활성 적재물에 러너 오른쪽 방향(음수면 왼쪽) 속도를 더해 손실 임박 경고나 적재물 손실 실패를 연출합니다.
 * PG.Showcase.CargoWatch <0|1|2>
 *   적재물이 바구니를 벗어나는 순간을 로그로 남깁니다. 바구니 면에 절반 넘게 박히거나, 테두리를 넘지 않고 옆면·바닥으로 빠지거나,
 *   떨어진 적재물이 바구니에 담기지 못하고 빗나가면 Warning이라 녹화 도구가 실패로 처리합니다.
 *   2는 프레임마다 바구니 기준 적재물 위치와 상대 속도, 생성 위치(CargoStack 앵커) 기준 위치,
 *   그리고 지난 프레임 시간 대비 물리가 실제로 진행한 시간과 스텝 수, 그 프레임의 손실 임박 비율(PHYS 줄)까지 남깁니다.
 * PG.Showcase.ProbeSegments [좌우 반폭 cm=200] [레인 간격 cm=100]
 *   현재 맵 RunDirector의 시작 세그먼트와 세그먼트 세트의 모든 클래스를 빈 공간에 하나씩 스폰해, 좌우 레인마다 50cm 간격으로 바닥을 트레이스한
 *   결과(길 높이 기준 틈 구간, 길 위 장애물, 레인별 길 표본 수)를 PROBE 로그로 남깁니다. 길 높이는 세그먼트 원점 근처 히트의 최빈값이라 아래 바다 메시는 틈으로 봅니다.
 *   튜토리얼 구간(Tutorial.Chapters) 설계용입니다.
 * 명령줄 -PGShowcaseSeed=<N>
 *   트랙 세그먼트 순서를 고정해 같은 스크립트를 같은 길에서 다시 녹화할 수 있게 합니다.
 */
namespace
{
	struct FPGFrameDump
	{
		FString Directory;
		int32 FrameCount = 0;
		int32 SavedFrames = 0;
		// Slate가 이번 프레임에 그린 창 픽셀을 받아 두는 버퍼와 그 크기입니다.
		TArray<FColor> Pixels;
		FIntPoint Size = FIntPoint::ZeroValue;
		FDelegateHandle BeginFrameHandle;
	};

	struct FPGShowcaseScript
	{
		// 실행 시각(초) 오름차순으로 정렬된 남은 명령입니다.
		TArray<TPair<double, FString>> PendingCommands;
		double ElapsedSeconds = 0.0;
		// WaitForRun 이후 런 시작을 기다리며 스크립트 시계를 멈춘 상태인지와, 그렇게 기다린 시간입니다.
		bool bWaitingForRun = false;
		double WaitedSeconds = 0.0;
		FDelegateHandle BeginFrameHandle;
	};

	FPGFrameDump GFrameDump;
	FPGShowcaseScript GShowcaseScript;
	FDelegateHandle GAutoPilotHandle;
	// 백그라운드에서 아직 쓰고 있는 PNG 수입니다.
	std::atomic<int32> GPendingFrameWrites{0};
	// 자동 조종이 고른 목표 좌우 위치(트랙 중앙 기준 cm)입니다.
	float GAutoPilotTargetOffset = 0.0f;

	UWorld* FindGameWorld()
	{
		return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
	}

	APGRunnerCharacter* FindRunner(const UWorld* World)
	{
		const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<APGRunnerCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	void TickFrameDump()
	{
		// 지난 프레임에 Slate가 채워 준 픽셀을 저장한다. PNG 압축은 한 프레임 시간을 넘기므로 백그라운드로 보내 녹화 속도가 실제 시간에서 밀리지 않게 한다.
		// 창 크기가 바뀌는 프레임에는 읽어 온 픽셀 수가 예약한 크기와 다를 수 있다. 그런 프레임은 버리고 다시 받는다.
		if (!GFrameDump.Pixels.IsEmpty() && GFrameDump.Pixels.Num() == GFrameDump.Size.X * GFrameDump.Size.Y)
		{
			++GPendingFrameWrites;
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
				[Pixels = MoveTemp(GFrameDump.Pixels), Size = GFrameDump.Size, Path = GFrameDump.Directory / FString::Printf(TEXT("frame_%05d.png"), GFrameDump.SavedFrames)]() mutable
			{
				// 창 백버퍼의 알파는 의미가 없으므로 불투명하게 채워야 뷰어와 인코더가 투명 이미지로 보지 않는다.
				for (FColor& Pixel : Pixels)
				{
					Pixel.A = 255;
				}

				TArray64<uint8> PngData;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PngData);
				FFileHelper::SaveArrayToFile(PngData, *Path);
				--GPendingFrameWrites;
			});
			++GFrameDump.SavedFrames;
		}
		GFrameDump.Pixels.Reset();

		UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport : nullptr;
		const TSharedPtr<SWindow> Window = ViewportClient ? ViewportClient->GetWindow() : nullptr;
		if (GFrameDump.SavedFrames >= GFrameDump.FrameCount || !Window.IsValid() || !ViewportClient->Viewport)
		{
			FCoreDelegates::OnBeginFrame.Remove(GFrameDump.BeginFrameHandle);
			GEngine->bUseFixedFrameRate = false;

			// 아직 쓰고 있는 PNG가 끝난 뒤 종료해야 마지막 프레임들이 빠지지 않는다.
			while (GPendingFrameWrites > 0)
			{
				FPlatformProcess::Sleep(0.01f);
			}
			UE_LOG(LogPGShowcase, Display, TEXT("Frame dump finished: %d frames in %s"), GFrameDump.SavedFrames, *GFrameDump.Directory);
			GFrameDump = FPGFrameDump();

			// 무인 실행에서 녹화가 끝나면 스스로 종료해, 호출한 스크립트가 인코딩 단계로 넘어가게 한다.
			FPlatformMisc::RequestExit(false);
			return;
		}

		// 스크린샷 요청(Shot ShowUI)은 창을 한 번 더 그려 위젯 Tick이 프레임당 두 번 돌고 UI 애니메이션이 2배속으로 찍힌다.
		// 대신 이번 프레임의 정규 Slate 그리기가 끝날 때 UI까지 합성된 창 픽셀을 버퍼로 읽어 오도록 예약만 해 둔다.
		GFrameDump.Size = ViewportClient->Viewport->GetSizeXY();
		FSlateApplication::Get().GetRenderer()->PrepareToTakeScreenshot(FIntRect(FIntPoint::ZeroValue, GFrameDump.Size), &GFrameDump.Pixels, Window.Get());
	}

	void StartFrameDump(const TArray<FString>& Args)
	{
		// 창 전체가 게임 화면인 -game 실행을 전제로 찍고 끝나면 프로세스를 종료한다.
		// 에디터(PIE 포함)에서는 엉뚱한 영역이 찍히고 저장 확인 없이 에디터가 꺼지므로 시작하지 않는다.
		if (GIsEditor)
		{
			UE_LOG(LogPGShowcase, Warning, TEXT("PG.Showcase.DumpFrames only runs in a -game process (Tools/Record-Showcase.ps1), not in the editor or PIE."));
			return;
		}

		const float FramesPerSecond = Args.Num() >= 3 ? FCString::Atof(*Args[1]) : 0.0f;
		const int32 FrameCount = Args.Num() >= 3 ? FCString::Atoi(*Args[2]) : 0;
		if (FramesPerSecond <= 0.0f || FrameCount <= 0 || GFrameDump.FrameCount > 0)
		{
			UE_LOG(LogPGShowcase, Warning, TEXT("Usage: PG.Showcase.DumpFrames <Directory> <Fps> <FrameCount> (only one dump at a time)"));
			return;
		}

		GFrameDump.Directory = Args[0];
		GFrameDump.FrameCount = FrameCount;
		IFileManager::Get().MakeDirectory(*GFrameDump.Directory, true);

		// 위젯 애니메이션은 실제 시간으로 Tick한다. 기다리지 않는 고정 스텝(FApp::SetUseFixedTimeStep)을 쓰면 UI만 다른 속도로 찍히므로,
		// 게임 시간을 프레임마다 1/FPS로 고정하면서 실제 시간도 그 속도에 맞춰 기다리는 엔진 고정 프레임레이트를 쓴다.
		GEngine->bUseFixedFrameRate = true;
		GEngine->FixedFrameRate = FramesPerSecond;
		GFrameDump.BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddStatic(&TickFrameDump);
	}

	// -PGShowcaseSeed=<N>: 월드 초기화 때 전역 난수를 고정해, RunDirector가 BeginPlay에서 뽑는 트랙 시드가 실행할 때마다 같게 나오도록 한다.
	// 같은 길에서 다시 녹화할 수 있으므로 자동 조종이 끝까지 버티는 시드를 골라 쓸 수 있다.
	// 이벤트·배달 서브시스템은 이 시점보다 먼저 초기화되어 고정되지 않는다. 이벤트는 PG.Showcase.RunEvent로 직접 지정한다.
	struct FPGShowcaseSeed
	{
		FPGShowcaseSeed()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld*, const UWorld::InitializationValues)
			{
				int32 Seed = 0;
				if (FParse::Value(FCommandLine::Get(), TEXT("PGShowcaseSeed="), Seed))
				{
					FMath::RandInit(Seed);
				}
			});
		}
	} GShowcaseSeed;

	void TickShowcaseScript()
	{
		UWorld* World = FindGameWorld();
		if (GShowcaseScript.bWaitingForRun)
		{
			const APGGameModeBase* GameMode = World ? World->GetAuthGameMode<APGGameModeBase>() : nullptr;
			if (!GameMode || GameMode->GetRunState() != EPGRunState::Running)
			{
				// 런이 끝내 시작되지 않으면 화면 없는 프로세스가 계속 남으므로 포기하고 종료한다. 에디터에서는 스크립트만 멈춘다.
				constexpr double MaxWaitSeconds = 60.0;
				GShowcaseScript.WaitedSeconds += FApp::GetDeltaTime();
				if (GShowcaseScript.WaitedSeconds > MaxWaitSeconds)
				{
					UE_LOG(LogPGShowcase, Error, TEXT("Showcase script aborted: the run did not start within %.0f seconds."), MaxWaitSeconds);
					FCoreDelegates::OnBeginFrame.Remove(GShowcaseScript.BeginFrameHandle);
					GShowcaseScript = FPGShowcaseScript();
					if (!GIsEditor)
					{
						FPlatformMisc::RequestExit(false);
					}
				}
				return;
			}
			GShowcaseScript.bWaitingForRun = false;
		}

		GShowcaseScript.ElapsedSeconds += FApp::GetDeltaTime();
		while (World && !GShowcaseScript.bWaitingForRun && !GShowcaseScript.PendingCommands.IsEmpty() && GShowcaseScript.PendingCommands[0].Key <= GShowcaseScript.ElapsedSeconds)
		{
			// 실행한 명령이 새 스크립트를 시작하거나 종료를 요청해도 안전하도록 먼저 목록에서 꺼낸다.
			const FString Command = GShowcaseScript.PendingCommands[0].Value;
			GShowcaseScript.PendingCommands.RemoveAt(0);
			UE_LOG(LogPGShowcase, Display, TEXT("[%.2fs] %s"), GShowcaseScript.ElapsedSeconds, *Command);
			GEngine->Exec(World, *Command);
		}

		if (GShowcaseScript.PendingCommands.IsEmpty())
		{
			FCoreDelegates::OnBeginFrame.Remove(GShowcaseScript.BeginFrameHandle);
			GShowcaseScript = FPGShowcaseScript();
		}
	}

	void PlayShowcaseScript(const TArray<FString>& Args)
	{
		TArray<FString> Lines;
		if (Args.IsEmpty() || !FFileHelper::LoadFileToStringArray(Lines, *Args[0]) || GShowcaseScript.BeginFrameHandle.IsValid())
		{
			UE_LOG(LogPGShowcase, Warning, TEXT("Usage: PG.Showcase.Play <ScriptFile> (lines: \"<seconds> <console command>\", only one script at a time)"));
			return;
		}

		for (const FString& Line : Lines)
		{
			FString TimeText;
			FString Command;
			const FString Trimmed = Line.TrimStartAndEnd();
			if (!Trimmed.StartsWith(TEXT("#")) && Trimmed.Split(TEXT(" "), &TimeText, &Command) && TimeText.IsNumeric())
			{
				GShowcaseScript.PendingCommands.Emplace(FCString::Atod(*TimeText), Command.TrimStart());
			}
		}

		GShowcaseScript.PendingCommands.StableSort([](const TPair<double, FString>& A, const TPair<double, FString>& B)
		{
			return A.Key < B.Key;
		});
		GShowcaseScript.BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddStatic(&TickShowcaseScript);
		UE_LOG(LogPGShowcase, Display, TEXT("Showcase script started: %d commands from %s"), GShowcaseScript.PendingCommands.Num(), *Args[0]);
	}

	// Balance 명령이 남긴 강제 중심 입력과 남은 시간. 자동 조종이 켜져 있으면 자동 조종 Tick이, 꺼져 있으면 TickBalanceOverride가 적용한다.
	float GBalanceOverrideInput = 0.0f;
	float GBalanceOverrideRemainingSeconds = 0.0f;
	FDelegateHandle GBalanceOverrideHandle;

	// 강제 입력이 남아 있으면 시간을 줄이고 그 값을, 없으면 주어진 입력을 그대로 돌려준다.
	float ApplyBalanceOverride(float Input)
	{
		if (GBalanceOverrideRemainingSeconds <= 0.0f)
		{
			return Input;
		}
		GBalanceOverrideRemainingSeconds -= static_cast<float>(FApp::GetDeltaTime());
		return GBalanceOverrideInput;
	}

	void TickAutoPilot()
	{
		APGRunnerCharacter* Runner = FindRunner(FindGameWorld());
		const UPGRunnerMovementComponent* Movement = Runner ? Cast<UPGRunnerMovementComponent>(Runner->GetCharacterMovement()) : nullptr;
		if (!Movement)
		{
			return;
		}

		const FVector Location = Runner->GetActorLocation();
		const float Offset = Movement->GetBalanceOffset();

		// 러너가 밟을 수 있는(Pawn을 막는) 면이 그 지점 발밑에 있는지 확인한다. 땅에 서 있을 때만 부르므로 지금 러너 높이가 기준이고,
		// 발밑 조금 아래까지만 확인해 수면 아래 바닥은 땅으로 치지 않는다. 널빤지 사이 틈으로 새지 않게 선이 아닌 구로 쓸어 보고,
		// 발판 가장자리에 걸친 자리를 길로 착각하지 않도록 좌우 양쪽에 모두 바닥이 있어야 디딜 수 있는 곳으로 본다.
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PGShowcaseGroundProbe), false, Runner);
		const auto HasGround = [&](float ForwardDistance, float TargetOffset)
		{
			constexpr float GroundProbeDepth = 140.0f;
			constexpr float FootingHalfWidth = 25.0f;
			constexpr float ProbeRadius = 25.0f;
			for (const float Side : { -FootingHalfWidth, FootingHalfWidth })
			{
				const FVector ProbeStart = Location + Movement->GetRunnerForwardVector() * ForwardDistance + Movement->GetRunnerRightVector() * (TargetOffset - Offset + Side);
				FHitResult GroundHit;
				if (!Runner->GetWorld()->SweepSingleByChannel(
					GroundHit, ProbeStart, ProbeStart - FVector(0.0f, 0.0f, GroundProbeDepth), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(ProbeRadius), QueryParams))
				{
					return false;
				}
			}
			return true;
		};

		// 트랙은 좌우 갈래로 나뉘고 끊긴다. 목표 위치마다 그쪽으로 갈 때 지나갈 지점들을 미리 찍어 길이 어디까지 이어지는지(샘플 번호)를 점수로 삼는다.
		// 점프 한 번으로 넘을 수 있는 틈은 이어진 길로 보고, 더 긴 틈에서 길이 끝난 것으로 친다.
		// 걸어갈 때는 낼 수 있는 좌우 속도로 조금씩 붙는 경로를, 지금 뛸 때는 공중에서 옮겨 갈 수 있는 만큼 옮겨 착지한 뒤 직진하는 경로를 본다.
		// ponytail: 아래 상수는 현재 튜닝값(전진 600, 지상 좌우 100cm/s, 점프 비거리 약 720cm, 공중 좌우 이동 약 200cm)에 맞춘 것이다.
		// 그 값이 크게 바뀌어 자동 조종이 떨어지기 시작하면 함께 조정한다.
		constexpr float LookAheadStep = 150.0f;
		constexpr int32 LookAheadSamples = 16;
		constexpr int32 MaxJumpableMisses = 4;
		constexpr float JumpDistance = 720.0f;
		constexpr float JumpLandingMargin = 50.0f;
		constexpr int32 JumpLandingSample = 5;
		constexpr float JumpLateralReach = 200.0f;
		constexpr float WalkLateralReachPerForward = 0.15f;
		const auto ScorePath = [&](float TargetOffset, bool bJumpNow, int32 MaxMisses)
		{
			const float JumpOffset = Offset + FMath::Clamp(TargetOffset - Offset, -JumpLateralReach, JumpLateralReach);
			if (bJumpNow && !(HasGround(JumpDistance - JumpLandingMargin, JumpOffset) && HasGround(JumpDistance + JumpLandingMargin, JumpOffset)))
			{
				// 착지할 자리 앞뒤에 바닥이 없는 점프는 갈 수 없는 길이다.
				return 0;
			}

			int32 Score = bJumpNow ? JumpLandingSample : 0;
			int32 Misses = 0;
			for (int32 Sample = Score + 1; Sample <= LookAheadSamples && Misses <= MaxMisses; ++Sample)
			{
				const float Distance = Sample * LookAheadStep;
				const float Reach = Distance * WalkLateralReachPerForward;
				if (HasGround(Distance, bJumpNow ? JumpOffset : Offset + FMath::Clamp(TargetOffset - Offset, -Reach, Reach)))
				{
					Score = Sample;
					Misses = 0;
				}
				else
				{
					++Misses;
				}
			}
			return Score;
		};

		// 기존 목표보다 길이 확실히 더 이어질 때만 목표를 바꿔 불필요한 좌우 이동으로 적재물을 흔들지 않고, 나은 후보끼리는 지금 위치에서 가까운 쪽을 고른다.
		const auto FindBestTarget = [&](bool bJumpNow, float& InOutTarget)
		{
			constexpr float CandidateRange = 350.0f;
			constexpr float CandidateStep = 50.0f;
			const float PreviousTarget = InOutTarget;
			int32 BestScore = ScorePath(PreviousTarget, bJumpNow, MaxJumpableMisses);
			for (float Candidate = -CandidateRange; Candidate <= CandidateRange; Candidate += CandidateStep)
			{
				const int32 Score = ScorePath(Candidate, bJumpNow, MaxJumpableMisses);
				if (Score > BestScore
					|| (Score == BestScore && InOutTarget != PreviousTarget && FMath::Abs(Candidate - Offset) < FMath::Abs(InOutTarget - Offset)))
				{
					BestScore = Score;
					InOutTarget = Candidate;
				}
			}
			return BestScore;
		};

		// 경로 예측은 땅에서 출발한다고 가정하므로, 공중에서는 뛰기 전에 정한 목표를 그대로 지켜 착지 지점을 벗어나지 않는다.
		if (!Movement->IsFalling())
		{
			const int32 BestScore = FindBestTarget(false, GAutoPilotTargetOffset);

			// 뛰는 시점이 늦으면 다음 틈에 착지할 수 있다. 그래서 뛰지 않고 갈 수 있는 길이 착지 거리 안에서 끊기면 착지할 자리에 바닥이 있는 첫 순간에 미리 뛰고,
			// 그런 순간이 없으면 발 앞이 끊길 때 뛴다. 걸어서는 길이 곧 끝나는데 옆 갈래로 넘어가면 더 멀리 갈 수 있을 때는 그쪽으로 플릭 점프한다.
			constexpr int32 JumpAdvantageSamples = 3;
			constexpr float JumpLookAhead = 80.0f;
			// 플릭은 좌우로 약 100cm를 한 번에 밀어 주므로, 그보다 가까운 목표에 쓰면 착지 지점을 지나쳐 발판 밖에 떨어진다.
			constexpr float FlickMinLateralDistance = 100.0f;
			float JumpTarget = GAutoPilotTargetOffset;
			int32 JumpScore = ScorePath(JumpTarget, true, MaxJumpableMisses);
			const bool bJumpOverGap = JumpScore > 0 && ScorePath(GAutoPilotTargetOffset, false, 0) < JumpLandingSample;
			if (!bJumpOverGap)
			{
				JumpScore = FindBestTarget(true, JumpTarget);
			}
			if (bJumpOverGap || JumpScore >= BestScore + JumpAdvantageSamples || !HasGround(JumpLookAhead, Offset))
			{
				const float LateralDistance = JumpTarget - Offset;
				Runner->RequestFlickJump(FMath::Abs(LateralDistance) >= FlickMinLateralDistance ? FMath::Sign(LateralDistance) : 0.0f);
				// 쿨타임 등으로 점프가 거절되면 목표를 바꾸지 않고 다음 프레임에 다시 판단한다.
				// 뛰었으면 공중에서는 바닥을 확인해 둔 착지 위치까지만 옮겨 간다.
				if (Movement->IsFalling())
				{
					GAutoPilotTargetOffset = Offset + FMath::Clamp(LateralDistance, -JumpLateralReach, JumpLateralReach);
				}
			}
		}

		// 목표와의 거리와 좌우 속도에 비례해 입력한다. 입력이 크면 적재물까지 쏟아지므로 완만한 계수를 쓴다.
		constexpr float OffsetGain = 0.01f;
		constexpr float VelocityGain = 0.006f;
		Runner->SetBalanceInput(ApplyBalanceOverride(FMath::Clamp((GAutoPilotTargetOffset - Offset) * OffsetGain - Movement->GetBalanceVelocity() * VelocityGain, -1.0f, 1.0f)));
	}

	void SetAutoPilot(const TArray<FString>& Args)
	{
		const bool bEnable = Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0;
		FCoreDelegates::OnBeginFrame.Remove(GAutoPilotHandle);
		GAutoPilotHandle = bEnable ? FCoreDelegates::OnBeginFrame.AddStatic(&TickAutoPilot) : FDelegateHandle();

		// 러너는 마지막 입력을 계속 유지하므로, 끌 때 비워 두지 않으면 마지막 조종 입력대로 계속 옆으로 밀려간다.
		if (APGRunnerCharacter* Runner = bEnable ? nullptr : FindRunner(FindGameWorld()))
		{
			Runner->SetBalanceInput(0.0f);
		}
	}

	void ScheduleRunEvent(const TArray<FString>& Args, UWorld* World)
	{
		EPGRunEventType EventType = EPGRunEventType::None;
		if (Args.Num() > 0)
		{
			EventType = Args[0].Equals(TEXT("Wind"), ESearchCase::IgnoreCase) ? EPGRunEventType::Wind
				: Args[0].Equals(TEXT("Snowstorm"), ESearchCase::IgnoreCase) ? EPGRunEventType::Snowstorm
				: Args[0].Equals(TEXT("MovingHazard"), ESearchCase::IgnoreCase) ? EPGRunEventType::MovingHazard
				: EPGRunEventType::None;
		}

		UPGRunEventSubsystem* RunEventSubsystem = World ? World->GetSubsystem<UPGRunEventSubsystem>() : nullptr;
		const float Direction = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		if (EventType == EPGRunEventType::None || !RunEventSubsystem || !RunEventSubsystem->ScheduleEventNow(EventType, Direction))
		{
			UE_LOG(LogPGShowcase, Warning, TEXT("Usage: PG.Showcase.RunEvent <Wind|Snowstorm|MovingHazard> [Direction] (only while a run is in progress)"));
		}
	}

	void TickBalanceOverride()
	{
		// 자동 조종이 켜져 있으면 그쪽 Tick이 강제 입력을 합친다. multicast delegate는 나중에 등록한 쪽이 먼저 불려 순서에 기댈 수 없다.
		if (GAutoPilotHandle.IsValid())
		{
			return;
		}

		APGRunnerCharacter* Runner = FindRunner(FindGameWorld());
		const bool bActive = GBalanceOverrideRemainingSeconds > 0.0f;
		if (Runner)
		{
			Runner->SetBalanceInput(ApplyBalanceOverride(0.0f));
		}
		if (!bActive)
		{
			FCoreDelegates::OnBeginFrame.Remove(GBalanceOverrideHandle);
			GBalanceOverrideHandle.Reset();
		}
	}

	void SetBalanceOverride(const TArray<FString>& Args)
	{
		GBalanceOverrideInput = FMath::Clamp(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, -1.0f, 1.0f);
		GBalanceOverrideRemainingSeconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.5f;
		if (!GAutoPilotHandle.IsValid() && !GBalanceOverrideHandle.IsValid())
		{
			GBalanceOverrideHandle = FCoreDelegates::OnBeginFrame.AddStatic(&TickBalanceOverride);
		}
	}

	void RequestShowcaseJump(const TArray<FString>& Args, UWorld* World)
	{
		APGRunnerCharacter* Runner = FindRunner(World);
		if (!Runner || !Runner->RequestJump())
		{
			// 녹화 도구는 Warning을 실패로 보므로 거절은 Display로만 남긴다. 프레임에서 점프 여부를 확인한다.
			UE_LOG(LogPGShowcase, Display, TEXT("PG.Showcase.Jump was refused (no runner, not running, airborne, or jump cooldown)."));
		}
	}

	void RequestShowcaseFlickJump(const TArray<FString>& Args, UWorld* World)
	{
		APGRunnerCharacter* Runner = FindRunner(World);
		if (Runner && Args.Num() > 0)
		{
			Runner->RequestFlickJump(FCString::Atof(*Args[0]) < 0.0f ? -1.0f : 1.0f);
		}
	}

	void OpenShowcaseShop(const TArray<FString>& Args, UWorld* World)
	{
		// 상점은 Home 위젯의 Shop 버튼 delegate로 열어 게임과 같은 경로(예열·페이드·카메라 블렌드)를 타고, 탭 전환은 상점 위젯의 탭 버튼 클릭과 같게 한다.
		const bool bMapsTab = Args.Num() > 0 && Args[0].Equals(TEXT("Maps"), ESearchCase::IgnoreCase);
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UPGShopScreenWidget::StaticClass(), false);
		if (Widgets.Num() > 0)
		{
			UButton* TabButton = Cast<UButton>(Widgets[0]->GetWidgetFromName(bMapsTab ? TEXT("MapsTabButton") : TEXT("CharactersTabButton")));
			if (TabButton)
			{
				TabButton->OnClicked.Broadcast();
			}
			return;
		}

		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UPGHomeScreenWidget::StaticClass(), false);
		if (Widgets.Num() > 0)
		{
			CastChecked<UPGHomeScreenWidget>(Widgets[0])->OnShopRequested.Broadcast();
		}
		else
		{
			UE_LOG(LogPGShowcase, Warning, TEXT("PG.Showcase.Shop was ignored (no Home or Shop screen is open)."));
		}
	}

	void ApplyCargoImpulse(const TArray<FString>& Args, UWorld* World)
	{
		const APGRunnerCharacter* Runner = FindRunner(World);
		if (UPGCargoStackComponent* CargoStack = Runner ? Runner->GetCargoStackComponent() : nullptr)
		{
			CargoStack->ApplyExternalLateralImpulse(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f);
		}
	}

	// 지난 프레임에 바구니 안에 있던 적재물, 바구니 면에 박혀 있다고 이미 알린 적재물, 한 번이라도 담겼거나 빗나갔다고 이미 알린 적재물,
	// 그리고 바구니의 속도를 구하기 위한 지난 프레임 위치입니다.
	TSet<TWeakObjectPtr<const AActor>> GCargoInBowl;
	TSet<TWeakObjectPtr<const AActor>> GCargoSunk;
	TSet<TWeakObjectPtr<const AActor>> GCargoSeated;
	FVector GPreviousBowlLocation = FVector::ZeroVector;
	// 지난 프레임까지 물리가 진행한 누적 시간과 스텝 수입니다.
	double GPreviousSolverTime = 0.0;
	int32 GPreviousSolverStep = 0;
	FDelegateHandle GCargoWatchHandle;
	bool bCargoWatchTrace = false;

	void TickCargoWatch()
	{
		UWorld* World = FindGameWorld();
		const APGRunnerCharacter* Runner = FindRunner(World);

		// 물리가 프레임 시간보다 덜 진행하면 적재물이 러너보다 뒤처진다. 프레임률을 낮춰 잴 때 두 시간을 나란히 비교할 수 있게 남긴다.
		Chaos::FPhysicsSolver* Solver = bCargoWatchTrace && World && World->GetPhysicsScene() ? World->GetPhysicsScene()->GetSolver() : nullptr;
		if (Solver)
		{
			const double SolverTime = Solver->GetSolverTime();
			const int32 SolverStep = Solver->GetCurrentFrame();
			// danger는 그 프레임의 손실 판정이 본 최대 수평 이탈 비율이다. 긴 프레임 하나가 판정 거리를 얼마나 늘리는지 프레임 시간과 나란히 본다.
			UE_LOG(LogPGShowcase, Display, TEXT("PHYS dt=%.4f sim=%.4f steps=%d danger=%.3f"), FApp::GetDeltaTime(), SolverTime - GPreviousSolverTime, SolverStep - GPreviousSolverStep,
				Runner ? Runner->GetCargoStackComponent()->GetCargoDangerRatio() : 0.0f);
			GPreviousSolverTime = SolverTime;
			GPreviousSolverStep = SolverStep;
		}

		TArray<AActor*> AttachedActors;
		if (Runner)
		{
			Runner->GetAttachedActors(AttachedActors);
		}

		// 러너에 부착되는 Actor는 바구니뿐이다. 그 안에서 충돌이 켜진 컴포넌트가 적재물을 받치는 면이다.
		const UPrimitiveComponent* BowlCollision = nullptr;
		for (const AActor* AttachedActor : AttachedActors)
		{
			TInlineComponentArray<UPrimitiveComponent*> Primitives(AttachedActor);
			for (const UPrimitiveComponent* Primitive : Primitives)
			{
				if (Primitive->IsCollisionEnabled())
				{
					BowlCollision = Primitive;
					break;
				}
			}
		}
		if (!BowlCollision)
		{
			GCargoInBowl.Reset();
			GCargoSunk.Reset();
			GCargoSeated.Reset();
			return;
		}

		// 바구니 기준 좌표에서 충돌 범위의 수평 반지름이 테두리, 위아래 끝이 테두리 높이와 바닥이다.
		const FTransform BowlTransform = BowlCollision->GetComponentTransform();
		const FBoxSphereBounds BowlBounds = BowlCollision->CalcBounds(FTransform(FQuat::Identity, FVector::ZeroVector, BowlTransform.GetScale3D()));
		const float RimRadius = BowlBounds.BoxExtent.X;
		const float RimHeight = BowlBounds.BoxExtent.Z;
		// 머리 위 높은 곳에서 떨어지는 적재물은 테두리 위 이 높이 안으로 내려온 뒤부터 담긴 것으로 본다.
		constexpr float SettleHeight = 30.0f;
		const float DeltaSeconds = FApp::GetDeltaTime();
		const FVector BowlVelocity = DeltaSeconds > UE_KINDA_SMALL_NUMBER ? (BowlTransform.GetLocation() - GPreviousBowlLocation) / DeltaSeconds : FVector::ZeroVector;
		GPreviousBowlLocation = BowlTransform.GetLocation();

		TSet<TWeakObjectPtr<const AActor>> CargoInBowl;
		for (TActorIterator<APGPhysicsCargoActor> It(World); It; ++It)
		{
			UStaticMeshComponent* CargoMesh = It->GetCargoMeshComponent();
			if (!CargoMesh)
			{
				continue;
			}

			const FVector Local = BowlTransform.InverseTransformPositionNoScale(CargoMesh->Bounds.Origin) - BowlBounds.Origin;
			const float Radius = FVector2D(Local.X, Local.Y).Size();
			const bool bInsideCylinder = Radius < RimRadius && Local.Z > -RimHeight;

			if (GCargoInBowl.Contains(*It))
			{
				if (bInsideCylinder)
				{
					CargoInBowl.Add(*It);

					// 담긴 적재물의 중심에서 바구니 아래 방향으로 쏜 선이 바구니 충돌에 닿지 않으면 중심이 이미 바구니 면 아래에 있다. 절반 넘게 박힌 것이다.
					// 되밀려 나오는 경우도 화면에서는 뚫고 내려간 것으로 보이므로 박혀 있는 동안 한 번 알린다.
					constexpr float ProbeDepth = 60.0f;
					FHitResult Hit;
					const FVector Centre = CargoMesh->Bounds.Origin;
					const FVector ProbeEnd = Centre - BowlTransform.GetUnitAxis(EAxis::Z) * ProbeDepth;
					const bool bCentreUnderSurface = Local.Z < RimHeight
						&& !BowlCollision->BodyInstance.LineTrace(Hit, Centre, ProbeEnd, /*bTraceComplex*/ true)
						&& !BowlCollision->BodyInstance.LineTrace(Hit, Centre, ProbeEnd, /*bTraceComplex*/ false);
					if (!bCentreUnderSurface)
					{
						GCargoSunk.Remove(*It);
					}
					else if (!GCargoSunk.Contains(*It))
					{
						GCargoSunk.Add(*It);
						UE_LOG(LogPGShowcase, Warning, TEXT("Cargo %s sank into the bowl surface (radius %.1f of %.1f, height %.1f of %.1f)"), *It->GetName(), Radius, RimRadius, Local.Z, RimHeight);
					}
				}
				// 테두리를 넘어 나가는 적재물은 중심이 테두리보다 높은 채로 반지름 밖으로 나간다.
				// 그보다 낮은 높이에서 옆으로 나갔거나 테두리 안쪽에서 바닥 아래로 내려갔다면 바구니 면을 뚫고 빠진 것이다.
				else if (Local.Z < RimHeight)
				{
					UE_LOG(LogPGShowcase, Warning, TEXT("Cargo %s passed through the bowl (radius %.1f of %.1f, height %.1f of %.1f)"), *It->GetName(), Radius, RimRadius, Local.Z, RimHeight);
				}
				else
				{
					UE_LOG(LogPGShowcase, Display, TEXT("Cargo %s left the bowl over the rim (radius %.1f of %.1f, height %.1f of %.1f)"), *It->GetName(), Radius, RimRadius, Local.Z, RimHeight);
				}
			}
			// 담기는 적재물은 위에서 테두리 높이를 지나 들어온다. 테두리 밖으로 떨어지는 적재물 위로 바구니가 다시 지나가도 담긴 것으로 세지 않는다.
			else if (bInsideCylinder && Local.Z >= RimHeight && Local.Z < RimHeight + SettleHeight)
			{
				CargoInBowl.Add(*It);
				GCargoSeated.Add(*It);
			}
			// 한 번도 담기지 못한 적재물이 테두리보다 낮은 높이에서 바구니 밖에 있으면 받지 못하고 흘린 것이다. 한 번만 알린다.
			else if (!bInsideCylinder && Local.Z < RimHeight && !GCargoSeated.Contains(*It))
			{
				GCargoSeated.Add(*It);
				UE_LOG(LogPGShowcase, Warning, TEXT("Cargo %s missed the bowl (radius %.1f of %.1f, height %.1f of %.1f)"), *It->GetName(), Radius, RimRadius, Local.Z, RimHeight);
			}

			if (bCargoWatchTrace)
			{
				const FVector RelativeVelocity = BowlTransform.InverseTransformVectorNoScale(CargoMesh->GetPhysicsLinearVelocity() - BowlVelocity);
				// 생성 위치(CargoStack 앵커) 기준 위치도 남겨, 물리로 바뀐 첫 프레임에 적재물이 앵커보다 얼마나 앞서는지 프레임률별로 비교한다.
				const FVector FromAnchor = Runner->GetCargoStackComponent()->GetComponentTransform().InverseTransformPositionNoScale(It->GetActorLocation());
				UE_LOG(LogPGShowcase, Display, TEXT("CARGO %s local=(%.1f,%.1f,%.1f) r=%.1f relV=(%.0f,%.0f,%.0f) bowlV=(%.0f,%.0f,%.0f) falling=%d anchor=(%.1f,%.1f,%.1f)"),
					*It->GetName(), Local.X, Local.Y, Local.Z, Radius, RelativeVelocity.X, RelativeVelocity.Y, RelativeVelocity.Z,
					BowlVelocity.X, BowlVelocity.Y, BowlVelocity.Z, Runner->GetCharacterMovement()->IsFalling() ? 1 : 0,
					FromAnchor.X, FromAnchor.Y, FromAnchor.Z);
			}
		}
		GCargoInBowl = MoveTemp(CargoInBowl);
	}

	void ProbeTrackSegments(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		TArray<TSubclassOf<APGTrackSegment>> SegmentClasses;
		for (TActorIterator<APGRunDirector> DirectorIt(World); DirectorIt; ++DirectorIt)
		{
			if (const APGTrackSegment* InitialSegment = DirectorIt->GetInitialSegment())
			{
				SegmentClasses.AddUnique(InitialSegment->GetClass());
			}
			if (const UPGTrackSegmentSet* SegmentSet = DirectorIt->GetSegmentSet())
			{
				for (const FPGTrackSegmentEntry& Entry : SegmentSet->SegmentEntries)
				{
					if (Entry.SegmentClass)
					{
						SegmentClasses.AddUnique(Entry.SegmentClass);
					}
				}
			}
		}

		// 레벨 지오메트리와 겹치지 않는 높이에 스폰해 세그먼트 자신(과 붙은 액터)만 트레이스에 잡히게 한다.
		const FVector Origin(0.0f, 0.0f, 100000.0f);
		const float HalfWidth = Args.Num() > 0 ? FMath::Max(0.0f, FCString::Atof(*Args[0])) : 200.0f;
		const float LaneStep = Args.Num() > 1 ? FMath::Max(10.0f, FCString::Atof(*Args[1])) : 100.0f;
		TArray<float> LaneOffsets;
		for (float Lane = -HalfWidth; Lane <= HalfWidth + KINDA_SMALL_NUMBER; Lane += LaneStep)
		{
			LaneOffsets.Add(Lane);
		}
		constexpr float StepX = 50.0f;
		constexpr float ObstacleHeight = 40.0f;
		constexpr float GapDepth = 100.0f;
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		for (TSubclassOf<APGTrackSegment> SegmentClass : SegmentClasses)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APGTrackSegment* Segment = World->SpawnActor<APGTrackSegment>(SegmentClass, FTransform(Origin), SpawnParameters);
			if (!Segment)
			{
				UE_LOG(LogPGShowcase, Warning, TEXT("PROBE %s: spawn failed"), *GetNameSafe(SegmentClass));
				continue;
			}
			Segment->ActivateFromPool(FTransform(Origin));

			const float Length = Segment->GetSegmentLength();
			const int32 StepCount = FMath::CeilToInt(Length / StepX) + 1;
			TArray<TOptional<float>> Heights;
			TArray<FString> HitNames;
			Heights.SetNum(StepCount * LaneOffsets.Num());
			HitNames.SetNum(Heights.Num());
			TMap<int32, int32> Histogram;
			for (int32 LaneIndex = 0; LaneIndex < LaneOffsets.Num(); ++LaneIndex)
			{
				for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
				{
					const FVector Start = Origin + FVector(StepIndex * StepX, LaneOffsets[LaneIndex], 400.0f);
					FHitResult Hit;
					if (World->LineTraceSingleByObjectType(Hit, Start, Start - FVector(0.0f, 0.0f, 1400.0f), ObjectParams)
						&& Hit.GetActor() && (Hit.GetActor() == Segment || Hit.GetActor()->IsAttachedTo(Segment)))
					{
						const float RelativeZ = Hit.ImpactPoint.Z - Origin.Z;
						Heights[LaneIndex * StepCount + StepIndex] = RelativeZ;
						HitNames[LaneIndex * StepCount + StepIndex] = Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT("?");
						// 세그먼트 안의 바다 메시는 길보다 수 m 아래라 길 높이 후보에서 뺀다. 길은 세그먼트 원점(시작점) 높이 근처다.
						if (RelativeZ > -150.0f)
						{
							++Histogram.FindOrAdd(FMath::RoundToInt(RelativeZ / 10.0f) * 10);
						}
					}
				}
			}

			int32 FloorZ = 0;
			int32 FloorCount = -1;
			for (const TPair<int32, int32>& Bucket : Histogram)
			{
				if (Bucket.Value > FloorCount)
				{
					FloorZ = Bucket.Key;
					FloorCount = Bucket.Value;
				}
			}

			UE_LOG(LogPGShowcase, Display, TEXT("PROBE %s len=%.0f floorZ=%d samples=%d"), *SegmentClass->GetName(), Length, FloorZ, FloorCount);
			for (int32 LaneIndex = 0; LaneIndex < LaneOffsets.Num(); ++LaneIndex)
			{
				FString Gaps;
				FString Obstacles;
				int32 GapStart = INDEX_NONE;
				int32 ObstacleStart = INDEX_NONE;
				float ObstacleTop = 0.0f;
				FString ObstacleName;
				int32 FloorSamples = 0;
				for (int32 StepIndex = 0; StepIndex <= StepCount; ++StepIndex)
				{
					const TOptional<float> Height = StepIndex < StepCount ? Heights[LaneIndex * StepCount + StepIndex] : TOptional<float>(FloorZ);
					const bool bGap = !Height.IsSet() || Height.GetValue() < FloorZ - GapDepth;
					const bool bObstacle = Height.IsSet() && Height.GetValue() > FloorZ + ObstacleHeight;
					FloorSamples += (!bGap && !bObstacle) ? 1 : 0;
					if (bGap && GapStart == INDEX_NONE)
					{
						GapStart = StepIndex;
					}
					else if (!bGap && GapStart != INDEX_NONE)
					{
						Gaps += FString::Printf(TEXT(" %d-%d"), FMath::RoundToInt(GapStart * StepX), FMath::RoundToInt(StepIndex * StepX));
						GapStart = INDEX_NONE;
					}
					if (bObstacle)
					{
						if (ObstacleStart == INDEX_NONE)
						{
							ObstacleStart = StepIndex;
							ObstacleTop = 0.0f;
							ObstacleName = HitNames[LaneIndex * StepCount + StepIndex];
						}
						ObstacleTop = FMath::Max(ObstacleTop, Height.GetValue() - FloorZ);
					}
					else if (ObstacleStart != INDEX_NONE)
					{
						Obstacles += FString::Printf(TEXT(" %d-%d(+%.0f %s)"), FMath::RoundToInt(ObstacleStart * StepX), FMath::RoundToInt(StepIndex * StepX), ObstacleTop, *ObstacleName);
						ObstacleStart = INDEX_NONE;
					}
				}
				UE_LOG(LogPGShowcase, Display, TEXT("PROBE   y=%+.0f floor=%d/%d gaps=[%s ] obstacles=[%s ]"), LaneOffsets[LaneIndex], FloorSamples, StepCount, *Gaps, *Obstacles);
			}
			Segment->Destroy();
		}
		UE_LOG(LogPGShowcase, Display, TEXT("PROBE DONE (%d classes)"), SegmentClasses.Num());
	}

	void SetCargoWatch(const TArray<FString>& Args)
	{
		const int32 Level = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		bCargoWatchTrace = Level >= 2;
		FCoreDelegates::OnBeginFrame.Remove(GCargoWatchHandle);
		GCargoWatchHandle = Level > 0 ? FCoreDelegates::OnBeginFrame.AddStatic(&TickCargoWatch) : FDelegateHandle();
		GCargoInBowl.Reset();
		GCargoSunk.Reset();
		GCargoSeated.Reset();
	}

	FAutoConsoleCommand GCargoWatchCommand(
		TEXT("PG.Showcase.CargoWatch"),
		TEXT("PG.Showcase.CargoWatch <0|1|2>: logs when cargo leaves the bowl; a Warning if it passed through the bowl surface. 2 also traces cargo positions every frame."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetCargoWatch));

	FAutoConsoleCommand GDumpFramesCommand(
		TEXT("PG.Showcase.DumpFrames"),
		TEXT("PG.Showcase.DumpFrames <Directory> <Fps> <FrameCount>: saves the game view with UI as numbered PNG frames at a fixed frame rate, then exits."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&StartFrameDump));

	FAutoConsoleCommand GAutoPilotCommand(
		TEXT("PG.Showcase.AutoPilot"),
		TEXT("PG.Showcase.AutoPilot <0|1>: steers and jumps for the runner so that a run continues without player input."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetAutoPilot));

	FAutoConsoleCommandWithWorldAndArgs GRunEventCommand(
		TEXT("PG.Showcase.RunEvent"),
		TEXT("PG.Showcase.RunEvent <Wind|Snowstorm|MovingHazard> [Direction]: starts that run event now, beginning with its preview."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ScheduleRunEvent));

	FAutoConsoleCommand GBalanceCommand(
		TEXT("PG.Showcase.Balance"),
		TEXT("PG.Showcase.Balance <Input -1..1> [Seconds]: forces the runner balance input for a while, overriding AutoPilot."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetBalanceOverride));

	FAutoConsoleCommandWithWorldAndArgs GJumpCommand(
		TEXT("PG.Showcase.Jump"),
		TEXT("PG.Showcase.Jump: requests one runner jump as the player would."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RequestShowcaseJump));

	FAutoConsoleCommandWithWorldAndArgs GFlickJumpCommand(
		TEXT("PG.Showcase.FlickJump"),
		TEXT("PG.Showcase.FlickJump <-1|1>: requests one lateral flick jump as the player would."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RequestShowcaseFlickJump));

	FAutoConsoleCommandWithWorldAndArgs GCargoImpulseCommand(
		TEXT("PG.Showcase.CargoImpulse"),
		TEXT("PG.Showcase.CargoImpulse <Velocity>: adds lateral velocity (cm/s, runner right is positive) to every active cargo."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ApplyCargoImpulse));

	FAutoConsoleCommand GPlayScriptCommand(
		TEXT("PG.Showcase.Play"),
		TEXT("PG.Showcase.Play <ScriptFile>: runs timed console commands from a text file (\"<seconds> <command>\" per line)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&PlayShowcaseScript));

	FAutoConsoleCommand GWaitForRunCommand(
		TEXT("PG.Showcase.WaitForRun"),
		TEXT("PG.Showcase.WaitForRun: pauses the showcase script clock until the run starts."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			GShowcaseScript.bWaitingForRun = true;
		}));

	FAutoConsoleCommandWithWorldAndArgs GShopCommand(
		TEXT("PG.Showcase.Shop"),
		TEXT("PG.Showcase.Shop [Characters|Maps]: opens the shop like the Home Shop button, or switches the tab when the shop is open."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&OpenShowcaseShop));

	FAutoConsoleCommand GBackCommand(
		TEXT("PG.Showcase.Back"),
		TEXT("PG.Showcase.Back: simulates the Android back button (APGPlayerController::HandleBackRequested)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UWorld* World = FindGameWorld();
			if (APGPlayerController* PlayerController = World ? Cast<APGPlayerController>(World->GetFirstPlayerController()) : nullptr)
			{
				PlayerController->HandleBackRequested();
			}
		}));

	FAutoConsoleCommand GBackgroundCommand(
		TEXT("PG.Showcase.Background"),
		TEXT("PG.Showcase.Background: simulates the app losing focus (ApplicationWillDeactivateDelegate) so the run auto-pauses."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
		}));

	FAutoConsoleCommand GContinueCommand(
		TEXT("PG.Showcase.Continue"),
		TEXT("PG.Showcase.Continue: does what the result screen's Continue button does (APGGameModeBase::RequestContinueRun)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UWorld* World = FindGameWorld();
			APGGameModeBase* GameMode = World ? World->GetAuthGameMode<APGGameModeBase>() : nullptr;
			if (!GameMode || !GameMode->RequestContinueRun())
			{
				UE_LOG(LogPGShowcase, Warning, TEXT("Continue was refused (run state, continue count or receipt balance)."));
			}
		}));

	FAutoConsoleCommand GResultButtonCommand(
		TEXT("PG.Showcase.ResultButton"),
		TEXT("PG.Showcase.ResultButton <WidgetName>: clicks that button of the open run result screen as the player would."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UWorld* World = FindGameWorld();
			TArray<UUserWidget*> Widgets;
			if (World)
			{
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UPGRunResultWidget::StaticClass(), false);
			}
			UButton* Button = Widgets.Num() > 0 && Args.Num() > 0 ? Cast<UButton>(Widgets[0]->GetWidgetFromName(FName(*Args[0]))) : nullptr;
			if (!Button || !Button->GetIsEnabled() || !Button->IsVisible())
			{
				UE_LOG(LogPGShowcase, Warning, TEXT("PG.Showcase.ResultButton %s was refused (no result screen, unknown button, or the button is disabled or hidden)."), Args.Num() > 0 ? *Args[0] : TEXT("?"));
				return;
			}
			Button->OnClicked.Broadcast();
		}));

	FAutoConsoleCommand GReceiptsCommand(
		TEXT("PG.Showcase.Receipts"),
		TEXT("PG.Showcase.Receipts <Amount>: adds receipts to the saved wallet so a Continue can be afforded."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UWorld* World = FindGameWorld();
			UPGUserDataSubsystem* UserData = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPGUserDataSubsystem>() : nullptr;
			if (!UserData || Args.Num() == 0 || !UserData->TryAddReceipts(FCString::Atoi(*Args[0]), TEXT("Showcase")))
			{
				UE_LOG(LogPGShowcase, Warning, TEXT("PG.Showcase.Receipts needs a positive amount and a saved wallet."));
			}
		}));

	FAutoConsoleCommand GRunReceiptsCommand(
		TEXT("PG.Showcase.RunReceipts"),
		TEXT("PG.Showcase.RunReceipts <Amount>: adds receipts to the current run (like picking them up) so the result screen offers the double-receipts ad."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			APGRunnerCharacter* Runner = FindRunner(FindGameWorld());
			UPGRunProgressComponent* Progress = Runner ? Runner->GetRunProgressComponent() : nullptr;
			const int32 Amount = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
			if (!Progress || Amount <= 0)
			{
				UE_LOG(LogPGShowcase, Warning, TEXT("PG.Showcase.RunReceipts needs a positive amount and a runner."));
				return;
			}
			Progress->AddReceipts(Amount, TEXT("Showcase"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GProbeSegmentsCommand(
		TEXT("PG.Showcase.ProbeSegments"),
		TEXT("PG.Showcase.ProbeSegments: spawns every track segment class of this map once and logs floor gaps, obstacles and lane coverage (PROBE lines)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ProbeTrackSegments));

	FAutoConsoleCommand GLanguageCommand(
		TEXT("PG.Showcase.Language"),
		TEXT("PG.Showcase.Language <culture>: switches the game language (ko, en) without saving it."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				UKismetInternationalizationLibrary::SetCurrentCulture(Args[0], false);
			}
		}));
}

#endif
