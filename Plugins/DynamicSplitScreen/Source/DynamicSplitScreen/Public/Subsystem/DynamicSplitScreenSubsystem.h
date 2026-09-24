// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Engine/EngineTypes.h"
#include "DynamicSplitScreenSubsystem.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class ADynamicSplitScreenCameraProxy;
class UDynamicSplitScreenViewportClient;

/**
 * ViewFamily based split-screen controller.
 *
 * Every frame it reads the other player's camera from ADynamicSplitScreenCameraProxy,
 * smooths it, and pushes it to UDynamicSplitScreenViewportClient as the secondary view.
 * Split <-> full screen transitions lerp the width of the main view rect.
 *
 * Full screen / split changes only affect *this machine's* screen.
 * Tunables can be overridden in DefaultGame.ini under [/Script/DynamicSplitScreen.DynamicSplitScreenSubsystem].
 */
UCLASS(Config = Game)
class DYNAMICSPLITSCREEN_API UDynamicSplitScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	// ── Split screen ──

	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void EnableSplitScreen();

	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void DisableSplitScreen();

	UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
	bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

	/** Animates this machine's main view to full screen. */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void TransitionToFullScreen();

	/** Animates this machine's view back to split screen. */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void TransitionToSplitScreen();

	/** True once the full screen transition has completed. */
	UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
	bool IsInFullScreenMode() const;

	/**
	 * Registers / releases a full screen request. Full screen stays on while at least one requester remains,
	 * and split screen returns only when all of them are gone (destroyed requesters are purged automatically).
	 *
	 * Volumes should use this instead of calling TransitionToFullScreen/TransitionToSplitScreen directly:
	 * when moving from overlapping volume A into volume B, A's EndOverlap can arrive after B's BeginOverlap
	 * and would otherwise turn off the full screen that B just requested.
	 */
	void RequestFullScreen(const UObject* Requester);
	void ReleaseFullScreen(const UObject* Requester);

	/**
	 * Whether this machine's screen should react to the given character entering/leaving a volume.
	 * Transitions only change the local view, so a remote player must never change our screen.
	 * If bForEnteringPlayer is false, FixedPlayerIndex is compared with the local player's ControllerId.
	 */
	static bool ShouldLocalViewRespondTo(const ACharacter* Character, bool bForEnteringPlayer, int32 FixedPlayerIndex);

	/** If true (default) this machine's own view is on the right and the other player's view on the left. */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void SetMainViewOnRight(bool bOnRight);

	UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
	bool IsMainViewOnRight() const { return bMainViewOnRight; }

	/**
	 * Returns the secondary camera location and the pawn it follows while the secondary view is visible.
	 * Returns false when there is no secondary view or the screen is fully in full screen mode.
	 */
	bool TryGetVisibleSecondaryView(FVector& OutCameraLocation, APawn*& OutTarget) const;

	// ── Fade ──

	/** Fades both split views to/from black (0 = clear, 1 = black). Reset automatically on map load. */
	UFUNCTION(BlueprintCallable, Category = "Dynamic Split Screen")
	void StartScreenFade(float TargetFadeAlpha, float Duration);

protected:
	/** Split <-> full screen transition time (seconds) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float TransitionDuration = 0.5f;

	/** Location interp speed while the other player views through a level camera (Fixed / Follow volumes) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Smoothing")
	float LocationInterpSpeed = 25.f;

	/** FInterpTo speed of the secondary camera rotation */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Smoothing")
	float RotationInterpSpeed = 45.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Smoothing")
	float FOVInterpSpeed = 12.f;

	/** Interp speed of the arm length, so zoom in/out on the remote player looks smooth */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Smoothing")
	float ArmLengthInterpSpeed = 30.f;

	/**
	 * Interp speed of the anchor (remote character location).
	 * CharacterMovement network smoothing only applies to the mesh; the capsule steps at every server update.
	 * This absorbs the remaining steps. 60 is roughly 16 ms of lag.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Smoothing")
	float AnchorInterpSpeed = 60.f;

	/** If the anchor jumps further than this (cm), snap instead of interpolating (teleport / respawn). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Smoothing")
	float SnapDistance = 1500.f;

	/** Sphere sweep for the secondary camera so it does not clip through walls */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Collision")
	bool bDoCollisionTest = true;

	/** Sweep radius (same default as USpringArmComponent::ProbeSize) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Collision")
	float ProbeSize = 12.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen|Collision")
	TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	bool bMainViewOnRight = true;

	UPROPERTY(BlueprintReadOnly, Category = "Dynamic Split Screen")
	bool bSplitScreenActive = false;

private:
	/** Caches the viewport client (only valid when the Game Viewport Client Class is UDynamicSplitScreenViewportClient) */
	bool ResolveViewportClient();

	/** Finds the proxy that holds the *other* player's camera on this machine */
	ADynamicSplitScreenCameraProxy* ResolveRemoteProxy() const;

	APlayerController* ResolveLocalPlayerController() const;

	/**
	 * Resolves the pawn this machine controls and the other player's pawn in one go.
	 * The other pawn is found via GameState->PlayerArray (replicated everywhere) as the pawn of the
	 * PlayerState that is not ours. This follows possession changes, and pawns without a PlayerState
	 * (AI, decoys) are never picked.
	 */
	bool ResolveSplitViewPair(APawn*& OutLocalPawn, APawn*& OutRemotePawn) const;

	void PushSecondaryCamera();

	void ResetSecondaryState();

	TWeakObjectPtr<UDynamicSplitScreenViewportClient> CachedViewportClient;
	TWeakObjectPtr<ADynamicSplitScreenCameraProxy> CachedRemoteProxy;

	/** Transition state — 0 = split, 1 = full screen */
	float CurrentAlpha = 0.f;
	float TargetAlpha = 0.f;

	TArray<TWeakObjectPtr<const UObject>> FullScreenRequesters;

	/** Secondary camera smoothing state */
	bool bHasSmoothedSecondary = false;
	FVector  SmoothedAnchorLocation = FVector::ZeroVector;
	FVector  SmoothedSecondaryLocation = FVector::ZeroVector;
	FRotator SmoothedSecondaryRotation = FRotator::ZeroRotator;
	float    SmoothedSecondaryFOV = 90.f;
	float    SmoothedArmLength = 0.f;

	/**
	 * Last frame's secondary target. Never used to *decide* the target — only to
	 *   1) reset smoothing on the frame the target changes, and
	 *   2) bridge the one or two frames after a possession swap where PlayerState back pointers disagree.
	 */
	TWeakObjectPtr<APawn> LastGoodRemotePawn;
	int32 SecondaryHoldFrames = 0;
};
