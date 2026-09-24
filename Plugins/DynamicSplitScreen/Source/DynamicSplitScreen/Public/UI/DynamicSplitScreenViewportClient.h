// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "SceneTypes.h"	// FSceneViewStateReference
#include "DynamicSplitScreenViewportClient.generated.h"

class UCanvas;
class UWorld;
class APlayerController;
class FSceneView;
class FSceneViewFamilyContext;

/**
 * GameViewportClient that renders split screen by injecting a second FSceneView
 * directly into the ViewFamily, on top of a single LocalPlayer.
 *
 * No dummy LocalPlayer / PlayerController / SpectatorPawn is created.
 *
 * - The secondary camera is pushed every frame by UDynamicSplitScreenSubsystem via SetSecondaryView().
 * - While the secondary view is inactive, the engine's default Draw() is used (single view).
 * - bSwapLeftRight == true places the main view on the right and the secondary view on the left.
 * - FullscreenAlpha (0..1) lerps the main view's width from half screen to full screen.
 *
 * Setup: Project Settings > Engine > General Settings > Game Viewport Client Class = DynamicSplitScreenViewportClient
 */
UCLASS()
class DYNAMICSPLITSCREEN_API UDynamicSplitScreenViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	UDynamicSplitScreenViewportClient(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
	virtual void BeginDestroy() override;

	/**
	 * Reports UObjects held by SecondaryViewState (post process MID pool) to the GC.
	 * FSceneViewState is not a UObject, so without this the GC can delete MIDs that the
	 * render thread still uses as soon as a post process material affects the secondary view.
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** Updates the secondary view camera. Called every frame by the subsystem. */
	void SetSecondaryView(const FVector& InLocation, const FRotator& InRotation, float InFOV, float InAspectRatio = 16.f / 9.f);

	/** Disables the secondary view and falls back to the engine's default Draw(). */
	void ClearSecondaryView();

	bool IsSecondaryViewActive() const { return bSecondaryActive; }

	/** 0 = split (50/50), 1 = main view covers the whole screen */
	void SetFullscreenAlpha(float InAlpha) { FullscreenAlpha = FMath::Clamp(InAlpha, 0.f, 1.f); }
	float GetFullscreenAlpha() const { return FullscreenAlpha; }

	/** If true, main view = right / secondary view = left. Default true. */
	void SetSwapLeftRight(bool bInSwap) { bSwapLeftRight = bInSwap; }
	bool GetSwapLeftRight() const { return bSwapLeftRight; }

	/** Black fade drawn over both views at once. Independent of PlayerCameraManager fades. */
	void StartFade(float TargetAlpha, float Duration);
	float GetFadeAlpha() const { return FadeAlpha; }

private:
	/** Computes the left/right view rectangles (applies FullscreenAlpha and swap). */
	void ComputeViewRects(const FIntPoint& ViewportSize, FIntRect& OutMainRect, FIntRect& OutSecondaryRect) const;

	/** Builds the secondary FSceneView and adds it to the ViewFamily. */
	FSceneView* AddSecondarySceneView(FSceneViewFamilyContext& ViewFamily, const FIntRect& ViewRect);

	/** Audio listener update that the engine Draw() normally does every frame. */
	void UpdateAudioListener(UWorld* ListenerWorld, APlayerController* PC, const FSceneView* View);

	/** Restores the main LocalPlayer's Origin/Size to the full viewport once split ends. */
	void RestoreMainLocalPlayerViewport();

	void TickFade(float DeltaSeconds);
	void DrawFadeOverlay(FViewport* InViewport);
	void OnPostLoadMap(UWorld* LoadedWorld);

	/** Canvas used to draw the HUD (equivalent of the engine's CanvasObject) */
	UPROPERTY(Transient)
	TObjectPtr<UCanvas> SceneCanvasObject = nullptr;

	/** Cached canvas for HUD debug / console / on-screen messages in PostRender */
	UPROPERTY(Transient)
	TObjectPtr<UCanvas> DebugCanvasObject = nullptr;

	bool bSecondaryActive = false;

	FVector  SecondaryLocation = FVector::ZeroVector;
	FRotator SecondaryRotation = FRotator::ZeroRotator;
	float    SecondaryFOV = 90.f;

	/** Aspect ratio in which SecondaryFOV is defined as horizontal FOV (sender's camera component AspectRatio). */
	float    SecondaryAspectRatio = 16.f / 9.f;

	/** The split boundary is snapped to this pixel multiple (TSR tiles / ScreenPercentage rounding). */
	static constexpr int32 ViewRectAlignment = 8;

	float FullscreenAlpha = 0.f;
	bool  bSwapLeftRight = true;

	/** ViewState of the secondary view (TSR / Lumen / auto exposure history) */
	FSceneViewStateReference SecondaryViewState;

	float FadeAlpha = 0.f;
	float FadeTargetAlpha = 0.f;
	float FadeSpeed = 1.f;
	FDelegateHandle PostLoadMapHandle;
};
