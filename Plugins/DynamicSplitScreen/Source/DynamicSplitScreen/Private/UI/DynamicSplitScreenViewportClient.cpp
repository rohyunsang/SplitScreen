// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DynamicSplitScreenViewportClient.h"
#include "DynamicSplitScreen.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/Console.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"
#include "SceneManagement.h"	// FSceneViewStateInterface
#include "SceneViewExtension.h"
#include "RendererInterface.h"
#include "LegacyScreenPercentageDriver.h"
#include "Modules/ModuleManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "UnrealClient.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "Debug/DebugDrawService.h"
#include "EngineUtils.h"	// DrawStatsHUD
#include "Math/InverseRotationMatrix.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"	// GetTransientPackage

UDynamicSplitScreenViewportClient::UDynamicSplitScreenViewportClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDynamicSplitScreenViewportClient::OnPostLoadMap);
}

void UDynamicSplitScreenViewportClient::BeginDestroy()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	Super::BeginDestroy();
}

void UDynamicSplitScreenViewportClient::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDynamicSplitScreenViewportClient* This = CastChecked<UDynamicSplitScreenViewportClient>(InThis);
	if (FSceneViewStateInterface* Ref = This->SecondaryViewState.GetReference())
	{
		Ref->AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects(InThis, Collector);
}

// ── Fade ──

void UDynamicSplitScreenViewportClient::StartFade(float TargetAlpha, float Duration)
{
	FadeTargetAlpha = FMath::Clamp(TargetAlpha, 0.f, 1.f);
	FadeSpeed = (Duration > KINDA_SMALL_NUMBER) ? (1.f / Duration) : 1000.f;
}

void UDynamicSplitScreenViewportClient::TickFade(float DeltaSeconds)
{
	if (!FMath::IsNearlyEqual(FadeAlpha, FadeTargetAlpha))
	{
		FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, FadeTargetAlpha, DeltaSeconds, FadeSpeed);
	}
}

void UDynamicSplitScreenViewportClient::DrawFadeOverlay(FViewport* InViewport)
{
	if (!InViewport || FadeAlpha <= KINDA_SMALL_NUMBER) return;

	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();
	if (!DebugCanvas) return;

	const FIntPoint Size = InViewport->GetSizeXY();
	FCanvasTileItem Tile(FVector2D::ZeroVector, FVector2D(Size), FLinearColor(0.f, 0.f, 0.f, FadeAlpha));
	Tile.BlendMode = SE_BLEND_Translucent;
	DebugCanvas->DrawItem(Tile);
}

void UDynamicSplitScreenViewportClient::OnPostLoadMap(UWorld* /*LoadedWorld*/)
{
	FadeAlpha = 0.f;
	FadeTargetAlpha = 0.f;
}

// ── Secondary view ──

void UDynamicSplitScreenViewportClient::SetSecondaryView(const FVector& InLocation, const FRotator& InRotation, float InFOV, float InAspectRatio)
{
	SecondaryLocation = InLocation;
	SecondaryRotation = InRotation;
	SecondaryFOV = (InFOV > KINDA_SMALL_NUMBER) ? InFOV : 90.f;
	SecondaryAspectRatio = (InAspectRatio > KINDA_SMALL_NUMBER) ? InAspectRatio : (16.f / 9.f);
	bSecondaryActive = true;
}

void UDynamicSplitScreenViewportClient::ClearSecondaryView()
{
	bSecondaryActive = false;
	RestoreMainLocalPlayerViewport();
}

void UDynamicSplitScreenViewportClient::ComputeViewRects(const FIntPoint& ViewportSize, FIntRect& OutMainRect, FIntRect& OutSecondaryRect) const
{
	const float SmoothedAlpha = FMath::InterpEaseInOut(0.f, 1.f, FullscreenAlpha, 2.f);

	const int32 HalfX = FMath::Max(1, ViewportSize.X / 2);
	int32 MainWidth = static_cast<int32>(FMath::Lerp(static_cast<float>(HalfX), static_cast<float>(ViewportSize.X), SmoothedAlpha));

	// Snap the boundary to ViewRectAlignment. When ScreenPercentage != 100 each view rect is scaled
	// and rounded separately; an unaligned boundary leaves a 1-2 px column nobody writes (seam noise).
	if (ViewRectAlignment > 1 && MainWidth < ViewportSize.X)
	{
		MainWidth = (MainWidth / ViewRectAlignment) * ViewRectAlignment;
	}

	// Don't render a sliver: a very narrow view has an extreme aspect ratio and pops at the end of the transition.
	const int32 MinViewWidth = FMath::Min(ViewRectAlignment * 2, ViewportSize.X);
	if (MainWidth > ViewportSize.X - MinViewWidth)
	{
		MainWidth = ViewportSize.X;
	}
	else if (MainWidth < MinViewWidth)
	{
		MainWidth = MinViewWidth;
	}

	if (!bSwapLeftRight)
	{
		OutMainRect = FIntRect(0, 0, MainWidth, ViewportSize.Y);
		OutSecondaryRect = FIntRect(MainWidth, 0, ViewportSize.X, ViewportSize.Y);
	}
	else
	{
		const int32 SecondaryWidth = FMath::Max(0, ViewportSize.X - MainWidth);
		OutSecondaryRect = FIntRect(0, 0, SecondaryWidth, ViewportSize.Y);
		OutMainRect = FIntRect(SecondaryWidth, 0, ViewportSize.X, ViewportSize.Y);
	}
}

FSceneView* UDynamicSplitScreenViewportClient::AddSecondarySceneView(FSceneViewFamilyContext& ViewFamily, const FIntRect& ViewRect)
{
	UWorld* MyWorld = GetWorld();
	if (!MyWorld || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
	{
		return nullptr;
	}

	if (!SecondaryViewState.GetReference())
	{
		SecondaryViewState.Allocate(MyWorld->GetFeatureLevel());
	}

	FSceneViewInitOptions ViewInit;
	ViewInit.ViewFamily = &ViewFamily;
	ViewInit.SceneViewStateInterface = SecondaryViewState.GetReference();
	ViewInit.ViewActor = nullptr;
	ViewInit.PlayerIndex = INDEX_NONE;
	ViewInit.SetViewRectangle(ViewRect);
	ViewInit.BackgroundColor = FLinearColor::Black;
	ViewInit.OverlayColor = FLinearColor::Transparent;
	ViewInit.FOV = SecondaryFOV;
	ViewInit.DesiredFOV = SecondaryFOV;

	// View matrix (same pattern as ULocalPlayer::GetProjectionData)
	ViewInit.ViewOrigin = SecondaryLocation;
	ViewInit.ViewRotationMatrix = FInverseRotationMatrix(SecondaryRotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	// Projection matrix — use the same function as the main view so both halves have an identical FOV.
	// (FMinimalViewInfo::CalculateProjectionMatrix applies the aspect ratio a second time.)
	{
		EAspectRatioAxisConstraint AspectConstraint = AspectRatio_MajorAxisFOV;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
			{
				AspectConstraint = LP->AspectRatioAxisConstraint;
			}
		}

		FMinimalViewInfo MinView;
		MinView.Location = SecondaryLocation;
		MinView.Rotation = SecondaryRotation;
		MinView.FOV = SecondaryFOV;
		MinView.DesiredFOV = SecondaryFOV;
		MinView.AspectRatio = SecondaryAspectRatio;
		MinView.bConstrainAspectRatio = false;
		MinView.ProjectionMode = ECameraProjectionMode::Perspective;

		FSceneViewProjectionData ProjData;
		ProjData.ViewOrigin = SecondaryLocation;
		ProjData.ViewRotationMatrix = ViewInit.ViewRotationMatrix;
		ProjData.SetViewRectangle(ViewRect);

		FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(MinView, AspectConstraint, ViewRect, ProjData);
		ViewInit.ProjectionMatrix = ProjData.ProjectionMatrix;
	}

	FSceneView* SecondarySceneView = new FSceneView(ViewInit);
	SecondarySceneView->ViewLocation = SecondaryLocation;
	SecondarySceneView->ViewRotation = SecondaryRotation;

	SecondarySceneView->StartFinalPostprocessSettings(SecondaryLocation);
	SecondarySceneView->EndFinalPostprocessSettings(ViewInit);

	ViewFamily.Views.Add(SecondarySceneView);
	return SecondarySceneView;
}

void UDynamicSplitScreenViewportClient::UpdateAudioListener(UWorld* ListenerWorld, APlayerController* PC, const FSceneView* View)
{
	// The split path bypasses Super::Draw, which is where the engine updates the listener every frame.
	if (!ListenerWorld || !PC || !View) return;

	FAudioDeviceHandle ListenerAudioDevice = ListenerWorld->GetAudioDevice();
	if (!ListenerAudioDevice.IsValid()) return;

	FVector Location;
	FVector ProjFront;
	FVector ProjRight;
	PC->GetAudioListenerPosition(Location, ProjFront, ProjRight);

	FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));
	ListenerTransform.SetTranslation(Location);
	ListenerTransform.NormalizeRotation();

	const uint32 ViewportIndex = 0;
	ListenerAudioDevice->SetListener(ListenerWorld, ViewportIndex, ListenerTransform, View->bCameraCut ? 0.f : ListenerWorld->GetDeltaSeconds());

	FVector OverrideAttenuation;
	if (PC->GetAudioListenerAttenuationOverridePosition(OverrideAttenuation))
	{
		ListenerAudioDevice->SetListenerAttenuationOverride(ViewportIndex, OverrideAttenuation);
	}
	else
	{
		ListenerAudioDevice->ClearListenerAttenuationOverride(ViewportIndex);
	}
}

void UDynamicSplitScreenViewportClient::RestoreMainLocalPlayerViewport()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
		{
			LP->Origin = FVector2D::ZeroVector;
			LP->Size = FVector2D(1.f, 1.f);
		}
	}
}

// ── Draw ──

void UDynamicSplitScreenViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	UWorld* MyWorld = GetWorld();
	TickFade(MyWorld ? MyWorld->DeltaTimeSeconds : 0.016f);

	UGameInstance* GI = GetGameInstance();
	ULocalPlayer* MainLocalPlayer = GI ? GI->GetFirstGamePlayer() : nullptr;
	const FIntPoint ViewportSize = InViewport ? InViewport->GetSizeXY() : FIntPoint::ZeroValue;

	// Fall back to the engine Draw when there is no secondary view (single view / menus / lobby)
	const bool bCanDrawSplit =
		bSecondaryActive && MyWorld && GI && InViewport && SceneCanvas &&
		MainLocalPlayer && MainLocalPlayer->PlayerController && MyWorld->Scene &&
		ViewportSize.X > 0 && ViewportSize.Y > 0;

	if (!bCanDrawSplit)
	{
		RestoreMainLocalPlayerViewport();
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	FIntRect MainRect, SecondaryRect;
	ComputeViewRects(ViewportSize, MainRect, SecondaryRect);

	APlayerController* MainPC = MainLocalPlayer->PlayerController;

	// ===== Build the ViewFamily =====
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewport, MyWorld->Scene, EngineShowFlags)
		.SetRealtimeUpdate(true));

	ViewFamily.DebugDPIScale = GetDPIScale();
	ViewFamily.EngineShowFlags = EngineShowFlags;
	ViewFamily.ViewMode = EViewModeIndex(ViewModeIndex);	// follow console viewmode (wireframe / unlit ...)
	ViewFamily.bIsMainViewFamily = true;
	ViewFamily.Time = FGameTime::CreateUndilated(MyWorld->TimeSeconds, MyWorld->DeltaTimeSeconds);

	// ===== Gather SceneViewExtensions =====
	// Upscalers (DLSS / FSR) and OCIO silently turn off during split if they are not registered here.
	if (GEngine && GEngine->ViewExtensions.IsValid())
	{
		FSceneViewExtensionContext ViewExtensionContext(InViewport);
		ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);
		for (const FSceneViewExtensionRef& ViewExt : ViewFamily.ViewExtensions)
		{
			ViewExt->SetupViewFamily(ViewFamily);
		}
	}

	// ===== Main view (through the LocalPlayer) =====
	// Keep LocalPlayer Origin/Size on the main rect while split, so that ProjectWorldLocationToScreen /
	// DeprojectMousePosition match the area that is actually rendered.
	{
		const float InvW = 1.f / FMath::Max(1.f, static_cast<float>(ViewportSize.X));
		const float InvH = 1.f / FMath::Max(1.f, static_cast<float>(ViewportSize.Y));

		MainLocalPlayer->Origin = FVector2D(static_cast<float>(MainRect.Min.X) * InvW,
		                                    static_cast<float>(MainRect.Min.Y) * InvH);
		MainLocalPlayer->Size = FVector2D(static_cast<float>(MainRect.Width()) * InvW,
		                                  static_cast<float>(MainRect.Height()) * InvH);
	}

	FVector OutViewLocation = FVector::ZeroVector;
	FRotator OutViewRotation = FRotator::ZeroRotator;
	FSceneView* MainSceneView = MainLocalPlayer->CalcSceneView(&ViewFamily, OutViewLocation, OutViewRotation, InViewport);

	if (!MainSceneView)
	{
		// Projection data failed (e.g. right after a PC swap). Rendering zero views would crash / go black.
		RestoreMainLocalPlayerViewport();
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	MainSceneView->CameraConstrainedViewRect = MainSceneView->UnscaledViewRect;
	MainLocalPlayer->LastViewLocation = OutViewLocation;
	AddStreamingViewInfo(*MyWorld, *MainSceneView);	// texture streaming, otherwise mips never load
	MyWorld->LastRenderTime = MyWorld->GetTimeSeconds();
	UpdateAudioListener(MyWorld, MainPC, MainSceneView);

	// ===== Secondary view (built directly, no LocalPlayer) =====
	if (SecondaryRect.Width() > 0 && SecondaryRect.Height() > 0)
	{
		if (FSceneView* SecondarySceneView = AddSecondarySceneView(ViewFamily, SecondaryRect))
		{
			// CalcSceneView does this for the main view; do the same for the secondary view
			for (const FSceneViewExtensionRef& ViewExt : ViewFamily.ViewExtensions)
			{
				ViewExt->SetupView(ViewFamily, *SecondarySceneView);
			}

			SecondarySceneView->CameraConstrainedViewRect = SecondarySceneView->UnscaledViewRect;
			AddStreamingViewInfo(*MyWorld, *SecondarySceneView);
		}
	}

	// ===== Camera cut flag =====
	// The engine Draw clears this every frame; otherwise it stays true and TSR resets forever.
	bool bAnyPlayerCameraCut = false;
	if (APlayerCameraManager* CamMgr = MainPC->PlayerCameraManager)
	{
		bAnyPlayerCameraCut = CamMgr->bGameCameraCutThisFrame;
		CamMgr->bGameCameraCutThisFrame = false;
	}
	InViewport->SetCameraCut(bAnyPlayerCameraCut);

	// ===== FinalizeViews =====
	{
		TMap<ULocalPlayer*, FSceneView*> PlayerViewMap;
		PlayerViewMap.Add(MainLocalPlayer, MainSceneView);
		FinalizeViews(&ViewFamily, PlayerViewMap);
	}

	// ===== ScreenPercentage interface =====
	// The ViewFamily takes ownership of the driver and deletes it.
	if (!ViewFamily.GetScreenPercentageInterface())
	{
		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			ViewFamily,
			FLegacyScreenPercentageDriver::GetCVarResolutionFraction()));
	}

	// ===== Render =====
	SceneCanvas->Clear(FLinearColor::Black);

	if (!bDisableWorldRendering && ViewFamily.Views.Num() > 0)
	{
		IRendererModule& RendererModule = FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
		RendererModule.BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
	}

	ProcessScreenShots(InViewport);

	// ===== PostRender (HUD / console / on-screen debug) =====
	// The HUD is drawn in the main view rect. The canvas needs the SceneView, otherwise Canvas/AHUD Project() return 0.
	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();

	if (!SceneCanvasObject)
	{
		SceneCanvasObject = NewObject<UCanvas>(GetTransientPackage());
	}
	if (!DebugCanvasObject)
	{
		DebugCanvasObject = NewObject<UCanvas>(GetTransientPackage());
	}

	if (DebugCanvas)
	{
		DebugCanvasObject->Init(ViewportSize.X, ViewportSize.Y, MainSceneView, DebugCanvas);
	}

	{
		const FVector CanvasOrigin(FMath::TruncToFloat(static_cast<float>(MainSceneView->UnscaledViewRect.Min.X)),
		                           FMath::TruncToFloat(static_cast<float>(MainSceneView->UnscaledViewRect.Min.Y)), 0.f);

		SceneCanvasObject->Init(MainSceneView->UnscaledViewRect.Width(), MainSceneView->UnscaledViewRect.Height(), MainSceneView, SceneCanvas);

		SceneCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
		SceneCanvasObject->ApplySafeZoneTransform();

		// 1) Canvas HUD of the main LocalPlayer (UMG is drawn by Slate separately)
		if (AHUD* HUD = MainPC->GetHUD())
		{
			DebugCanvasObject->SceneView = MainSceneView;
			HUD->SetCanvas(SceneCanvasObject, DebugCanvasObject);
			HUD->PostRender();

			// Pointers can change during PostRender (e.g. BP breakpoints), restore them
			SceneCanvasObject->Canvas = SceneCanvas;
			DebugCanvasObject->Canvas = DebugCanvas;

			if (IsValid(MainPC))
			{
				HUD->SetCanvas(nullptr, nullptr);
			}
		}

		// 2) Debug drawing (DrawDebug* / ShowFlag based)
		if (DebugCanvas)
		{
			DebugCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
			UDebugDrawService::Draw(ViewFamily.EngineShowFlags, InViewport, MainSceneView, DebugCanvas, DebugCanvasObject, MainPC);
			DebugCanvas->PopTransform();
		}

		SceneCanvasObject->PopSafeZoneTransform();
		SceneCanvas->PopTransform();
	}

	if (DebugCanvas)
	{
		DebugCanvasObject->ApplySafeZoneTransform();

		// 3) Transition / title-safe
		PostRender(DebugCanvasObject);

		// 4) Console
		if (ViewportConsole)
		{
			ViewportConsole->PostRender_Console(DebugCanvasObject);
		}

		// 5) OnScreenDebugMessage
		if (GEngine)
		{
			GEngine->DrawOnscreenDebugMessages(MyWorld, InViewport, DebugCanvas, DebugCanvasObject, 40.0f, 100.0f);
		}

		// 6) stat fps / stat unit ...
		{
			FVector PlayerCameraLocation = FVector::ZeroVector;
			FRotator PlayerCameraRotation = FRotator::ZeroRotator;
			MainPC->GetPlayerViewPoint(PlayerCameraLocation, PlayerCameraRotation);

			DrawStatsHUD(MyWorld, InViewport, DebugCanvas, DebugCanvasObject,
			             DebugProperties, PlayerCameraLocation, PlayerCameraRotation);
		}

		DebugCanvasObject->PopSafeZoneTransform();
	}

	DrawFadeOverlay(InViewport);
}
