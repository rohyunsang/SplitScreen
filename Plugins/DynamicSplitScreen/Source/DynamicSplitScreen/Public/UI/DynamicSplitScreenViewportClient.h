// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TimerHandle.h"
#include "DynamicSplitScreenViewportClient.generated.h"

/**
 * Custom GameViewportClient for animated split-screen to full-screen transitions.
 */
UCLASS()
class DYNAMICSPLITSCREEN_API UDynamicSplitScreenViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	/** Start transition from Split Screen to Full Screen for a specific player */
	void StartFullScreenTransition(int32 PlayerIndex);

	/** Start transition from Full Screen to Split Screen */
	void StartSplitScreenTransition();

	/** Check if currently in full screen mode (including during transition) */
	bool IsInFullScreenMode() const { return bIsFullScreen || bTransitioning; }

protected:
	/** Override split screen layout to swap left/right if needed or apply lerp */
	virtual void LayoutPlayers() override;

private:
	/** Lerp update callback */
	void UpdateTransitionLerp();

	// ── Transition States ──
	bool bIsFullScreen = false;
	bool bTransitioning = false;
	bool bToFullScreen = false;			// true: Split->Full, false: Full->Split
	int32 FullScreenPlayerIndex = 0;	// Target player for full screen

	float TransitionAlpha = 0.f;		// 0=Split, 1=FullScreen

	/** Animation duration (seconds) */
	float TransitionDuration = 0.5f;

	FTimerHandle TransitionTimerHandle;
};
