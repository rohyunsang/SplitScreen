// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DynamicSplitScreenViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

void UDynamicSplitScreenViewportClient::LayoutPlayers()
{
	// Execute default engine layout
	Super::LayoutPlayers();

	// Only handle 2-player split screen
	const int32 NumPlayers = GetOuterUEngine()->GetNumGamePlayers(GetWorld());
	if (NumPlayers != 2) return;

	ULocalPlayer* Player0 = GetOuterUEngine()->GetGamePlayer(GetWorld(), 0);
	ULocalPlayer* Player1 = GetOuterUEngine()->GetGamePlayer(GetWorld(), 1);

	if (!Player0 || !Player1) return;

	// ── Full Screen Mode or Transitioning ──
	if (bIsFullScreen || bTransitioning)
	{
		// Determine full screen target and the other player
		ULocalPlayer* FullPlayer = (FullScreenPlayerIndex == 0) ? Player0 : Player1;
		ULocalPlayer* OtherPlayer = (FullScreenPlayerIndex == 0) ? Player1 : Player0;

		// Default split screen origins (Assuming swapped: P0 is on Right, P1 is on Left)
		FVector2D FullPlayerSplitOrigin = (FullScreenPlayerIndex == 0) ? FVector2D(0.5f, 0.f) : FVector2D(0.f, 0.f);
		FVector2D OtherPlayerSplitOrigin = (FullScreenPlayerIndex == 0) ? FVector2D(0.f, 0.f) : FVector2D(0.5f, 0.f);
		FVector2D SplitSize(0.5f, 1.f);

		// Full screen target: Subject=Full Screen, Other=Width 0 (Push left along the vertical line)
		FVector2D FullOriginTarget(0.f, 0.f);
		FVector2D FullSizeTarget(1.f, 1.f);
		FVector2D OtherSizeTarget(0.f, 1.f);	// Keep height, width 0

		// Apply EaseInOut curve
		const float SmoothedAlpha = FMath::InterpEaseInOut(0.f, 1.f, TransitionAlpha, 2.f);

		// Apply Lerp
		FullPlayer->Origin = FMath::Lerp(FullPlayerSplitOrigin, FullOriginTarget, SmoothedAlpha);
		FullPlayer->Size = FMath::Lerp(SplitSize, FullSizeTarget, SmoothedAlpha);

		OtherPlayer->Origin = OtherPlayerSplitOrigin;	// Fix Origin (keep it on the left)
		OtherPlayer->Size = FMath::Lerp(SplitSize, OtherSizeTarget, SmoothedAlpha);

		return;
	}

	// ── Normal Split Screen — Swap Left/Right Logic ──
	FVector2D Origin0 = Player0->Origin;
	FVector2D Size0   = Player0->Size;

	FVector2D Origin1 = Player1->Origin;
	FVector2D Size1   = Player1->Size;

	// Swap: Player 0 -> Player 1 position, Player 1 -> Player 0 position
	Player0->Origin = Origin1;
	Player0->Size   = Size1;

	Player1->Origin = Origin0;
	Player1->Size   = Size0;
}

void UDynamicSplitScreenViewportClient::StartFullScreenTransition(int32 PlayerIndex)
{
	if (bIsFullScreen && FullScreenPlayerIndex == PlayerIndex)
	{
		UE_LOG(LogTemp, Log, TEXT("DynamicSplitScreen: Already in full screen mode for Player %d"), PlayerIndex);
		return;
	}

	FullScreenPlayerIndex = PlayerIndex;
	bTransitioning = true;
	bToFullScreen = true;
	TransitionAlpha = 0.f;

	// Request transition loop
	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
		CurrentWorld->GetTimerManager().SetTimer(
			TransitionTimerHandle,
			FTimerDelegate::CreateUObject(this, &UDynamicSplitScreenViewportClient::UpdateTransitionLerp),
			0.016f,
			true
		);
	}

	UE_LOG(LogTemp, Log, TEXT("DynamicSplitScreen StartFullScreenTransition: Player %d, Duration %f"), PlayerIndex, TransitionDuration);
}

void UDynamicSplitScreenViewportClient::StartSplitScreenTransition()
{
	if (!bIsFullScreen && !bTransitioning)
	{
		UE_LOG(LogTemp, Log, TEXT("DynamicSplitScreen: Already in split screen mode"));
		return;
	}

	bTransitioning = true;
	bToFullScreen = false;
	TransitionAlpha = 1.f;	// Return from full screen

	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
		CurrentWorld->GetTimerManager().SetTimer(
			TransitionTimerHandle,
			FTimerDelegate::CreateUObject(this, &UDynamicSplitScreenViewportClient::UpdateTransitionLerp),
			0.016f,
			true
		);
	}

	UE_LOG(LogTemp, Log, TEXT("DynamicSplitScreen StartSplitScreenTransition: Restoring split screen"));
}

void UDynamicSplitScreenViewportClient::UpdateTransitionLerp()
{
	const float DeltaAlpha = 0.016f / FMath::Max(TransitionDuration, 0.01f);

	if (bToFullScreen)
	{
		TransitionAlpha = FMath::Clamp(TransitionAlpha + DeltaAlpha, 0.f, 1.f);

		if (TransitionAlpha >= 1.f)
		{
			// Complete
			bTransitioning = false;
			bIsFullScreen = true;

			if (UWorld* CurrentWorld = GetWorld())
			{
				CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
			}

			UE_LOG(LogTemp, Log, TEXT("DynamicSplitScreen Full screen transition complete for Player %d"), FullScreenPlayerIndex);
		}
	}
	else
	{
		TransitionAlpha = FMath::Clamp(TransitionAlpha - DeltaAlpha, 0.f, 1.f);

		if (TransitionAlpha <= 0.f)
		{
			// Complete
			bTransitioning = false;
			bIsFullScreen = false;

			if (UWorld* CurrentWorld = GetWorld())
			{
				CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
			}

			UE_LOG(LogTemp, Log, TEXT("DynamicSplitScreen Split screen transition complete"));
		}
	}
}
