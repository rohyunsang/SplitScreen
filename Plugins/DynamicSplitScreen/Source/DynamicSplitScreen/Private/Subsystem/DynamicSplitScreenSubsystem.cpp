// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "DynamicSplitScreen.h"
#include "UI/DynamicSplitScreenViewportClient.h"
#include "Actors/DynamicSplitScreenCameraProxy.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarDynamicSplitScreenDebugDraw(
	TEXT("DynamicSplitScreen.DebugDraw"),
	0,
	TEXT("1 = draw the secondary camera location (yellow sphere) and forward (red line) every frame."),
	ECVF_Cheat);

namespace DynamicSplitScreen
{
	/**
	 * How many frames to keep the previous secondary target when the other pawn cannot be resolved.
	 * Right after a possession swap the PlayerState back pointers disagree for a frame or two; turning the
	 * secondary view off there would flash full screen once.
	 */
	static constexpr int32 MaxSecondaryHoldFrames = 6;
}

void UDynamicSplitScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bSplitScreenActive = false;
	CurrentAlpha = 0.f;
	TargetAlpha = 0.f;
}

void UDynamicSplitScreenSubsystem::Deinitialize()
{
	if (UDynamicSplitScreenViewportClient* VC = CachedViewportClient.Get())
	{
		VC->ClearSecondaryView();
	}
	CachedViewportClient.Reset();
	ResetSecondaryState();
	Super::Deinitialize();
}

void UDynamicSplitScreenSubsystem::ResetSecondaryState()
{
	CachedRemoteProxy.Reset();
	LastGoodRemotePawn.Reset();
	SecondaryHoldFrames = 0;
	bHasSmoothedSecondary = false;
}

// ── Resolve helpers ──

bool UDynamicSplitScreenSubsystem::ResolveViewportClient()
{
	if (CachedViewportClient.IsValid())
	{
		return true;
	}

	// Use the GameInstance's viewport, not GEngine->GameViewport: with multiple PIE windows
	// the global one may belong to another PIE instance.
	UGameInstance* GI = GetGameInstance();
	UDynamicSplitScreenViewportClient* VC = GI ? Cast<UDynamicSplitScreenViewportClient>(GI->GetGameViewportClient()) : nullptr;
	if (!VC)
	{
		return false;
	}

	CachedViewportClient = VC;
	VC->SetSwapLeftRight(bMainViewOnRight);
	return true;
}

APlayerController* UDynamicSplitScreenSubsystem::ResolveLocalPlayerController() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return nullptr;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

ADynamicSplitScreenCameraProxy* UDynamicSplitScreenSubsystem::ResolveRemoteProxy() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return nullptr;

	// - Listen server: use the proxy owned by the remote client
	// - Client: use the server proxy (the host's camera)
	// - Standalone: no secondary view
	const ENetMode NetMode = World->GetNetMode();
	if (NetMode != NM_Client && NetMode != NM_ListenServer)
	{
		return nullptr;
	}

	const APlayerController* LocalPC = ResolveLocalPlayerController();

	for (TActorIterator<ADynamicSplitScreenCameraProxy> It(World); It; ++It)
	{
		ADynamicSplitScreenCameraProxy* Proxy = *It;
		if (!Proxy) continue;

		if (NetMode == NM_Client)
		{
			if (Proxy->IsServerProxy())
			{
				return Proxy;
			}
		}
		else if (!Proxy->IsServerProxy())
		{
			const APlayerController* OwnerPC = Cast<APlayerController>(Proxy->GetOwner());
			if (OwnerPC && OwnerPC != LocalPC)
			{
				return Proxy;
			}
		}
	}
	return nullptr;
}

bool UDynamicSplitScreenSubsystem::ResolveSplitViewPair(APawn*& OutLocalPawn, APawn*& OutRemotePawn) const
{
	OutLocalPawn = nullptr;
	OutRemotePawn = nullptr;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return false;

	APlayerController* LocalPC = ResolveLocalPlayerController();
	if (!LocalPC) return false;

	OutLocalPawn = LocalPC->GetPawn();

	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState) return false;

	const APlayerState* LocalPS = LocalPC->PlayerState;

	// This split screen is two-player only: the first PlayerState that is not ours is the other player.
	for (const TObjectPtr<APlayerState>& PlayerStatePtr : GameState->PlayerArray)
	{
		APlayerState* PS = PlayerStatePtr.Get();
		if (!IsValid(PS) || PS == LocalPS) continue;
		if (PS->IsABot() || PS->IsOnlyASpectator()) continue;

		APawn* Pawn = PS->GetPawn();
		if (IsValid(Pawn))
		{
			OutRemotePawn = Pawn;
			break;
		}
	}

	return OutRemotePawn != nullptr;
}

// ── Secondary camera ──

void UDynamicSplitScreenSubsystem::PushSecondaryCamera()
{
	UDynamicSplitScreenViewportClient* VC = CachedViewportClient.Get();
	if (!VC) return;

	ADynamicSplitScreenCameraProxy* Proxy = CachedRemoteProxy.Get();
	if (!Proxy)
	{
		Proxy = ResolveRemoteProxy();
		CachedRemoteProxy = Proxy;
	}

	if (!Proxy)
	{
		VC->ClearSecondaryView();
		bHasSmoothedSecondary = false;
		return;
	}

	// Re-resolve the target *every frame*. A cached pointer stays valid across possession swaps
	// but then points at the wrong pawn, and both halves end up showing the same character.
	APawn* LocalPawn = nullptr;
	APawn* PairRemotePawn = nullptr;
	ResolveSplitViewPair(LocalPawn, PairRemotePawn);

	// Never show our own pawn in the secondary view
	if (PairRemotePawn == LocalPawn)
	{
		PairRemotePawn = nullptr;
	}

	APawn* RemotePawn = nullptr;
	if (IsValid(PairRemotePawn))
	{
		RemotePawn = PairRemotePawn;
		SecondaryHoldFrames = 0;
	}
	else
	{
		APawn* HeldPawn = LastGoodRemotePawn.Get();
		if (IsValid(HeldPawn) && HeldPawn != LocalPawn &&
			SecondaryHoldFrames < DynamicSplitScreen::MaxSecondaryHoldFrames)
		{
			RemotePawn = HeldPawn;
			++SecondaryHoldFrames;
		}
	}

	if (!IsValid(RemotePawn))
	{
		LastGoodRemotePawn.Reset();
		SecondaryHoldFrames = 0;
		VC->ClearSecondaryView();
		bHasSmoothedSecondary = false;
		return;
	}

	// Target changed: cut the smoothing, otherwise the camera slides from the old pawn to the new one
	if (LastGoodRemotePawn.Get() != RemotePawn)
	{
		bHasSmoothedSecondary = false;
	}
	LastGoodRemotePawn = RemotePawn;

	const FDynamicSplitScreenCameraInfo& TargetCam = Proxy->GetReplicatedCamera();

	// Nothing received yet
	if (TargetCam.Rotation.IsNearlyZero() && TargetCam.ArmLength < KINDA_SMALL_NUMBER)
	{
		VC->ClearSecondaryView();
		bHasSmoothedSecondary = false;
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const float DT = World ? World->GetDeltaSeconds() : 0.016f;

	// ===== Anchor =====
	// Network smoothing only moves the mesh; the capsule (root, and therefore the SpringArm) steps at every
	// server update. Mesh world location minus its relative offset gives a *smoothed* actor location,
	// and the SpringArm's relative offset on top of that gives a smooth arm origin.
	USpringArmComponent* RemoteArm = RemotePawn->FindComponentByClass<USpringArmComponent>();
	const ACharacter* RemoteCharacter = Cast<ACharacter>(RemotePawn);
	const USkeletalMeshComponent* RemoteMesh = RemoteCharacter ? RemoteCharacter->GetMesh() : nullptr;

	FVector RawAnchor;
	if (RemoteMesh && RemoteArm)
	{
		const FQuat ActorQuat = RemotePawn->GetActorQuat();
		const FVector SmoothActorLoc = RemoteMesh->GetComponentLocation()
			- ActorQuat.RotateVector(RemoteMesh->GetRelativeLocation());
		RawAnchor = SmoothActorLoc + ActorQuat.RotateVector(RemoteArm->GetRelativeLocation());
	}
	else if (RemoteArm)
	{
		RawAnchor = RemoteArm->GetComponentLocation();
	}
	else
	{
		RawAnchor = RemotePawn->GetActorLocation();
	}

	// ===== Smoothing =====
	const float TgtFOV = (TargetCam.FOV > KINDA_SMALL_NUMBER) ? TargetCam.FOV : 90.f;
	const float TgtArm = (TargetCam.ArmLength > KINDA_SMALL_NUMBER)
		? TargetCam.ArmLength
		: (RemoteArm ? RemoteArm->TargetArmLength : 0.f);

	const float AnchorJump = FVector::Dist(SmoothedAnchorLocation, RawAnchor);
	const bool bSnap = !bHasSmoothedSecondary || AnchorJump > SnapDistance;
	if (bSnap)
	{
		SmoothedAnchorLocation = RawAnchor;
		SmoothedSecondaryRotation = TargetCam.Rotation;
		SmoothedSecondaryFOV = TgtFOV;
		SmoothedArmLength = TgtArm;
		bHasSmoothedSecondary = true;
	}
	else
	{
		SmoothedAnchorLocation = FMath::VInterpTo(SmoothedAnchorLocation, RawAnchor, DT, AnchorInterpSpeed);
		SmoothedSecondaryRotation = FMath::RInterpTo(SmoothedSecondaryRotation, TargetCam.Rotation, DT, RotationInterpSpeed);
		SmoothedSecondaryFOV = FMath::FInterpTo(SmoothedSecondaryFOV, TgtFOV, DT, FOVInterpSpeed);
		SmoothedArmLength = FMath::FInterpTo(SmoothedArmLength, TgtArm, DT, ArmLengthInterpSpeed);
	}

	// ===== Level camera (Fixed / Follow volumes, or any other view target) =====
	// The spring arm says nothing about where that camera is, so use the replicated location.
	if (!TargetCam.bUsesPawnCamera)
	{
		SmoothedSecondaryLocation = bSnap
			? TargetCam.Location
			: FMath::VInterpTo(SmoothedSecondaryLocation, TargetCam.Location, DT, LocationInterpSpeed);
	}
	else
	{
		// ===== Pawn camera =====
		// Always derive the location from the *smoothed rotation* (same order as USpringArmComponent).
		// Using the replicated location directly puts location and rotation through different filters,
		// and the view swims while the other player turns the camera.
		const FVector TargetOffset = RemoteArm ? RemoteArm->TargetOffset : FVector::ZeroVector;
		const FVector SocketOffset = RemoteArm ? RemoteArm->SocketOffset : FVector::ZeroVector;
		const FVector ArmOrigin = SmoothedAnchorLocation + TargetOffset;

		const FVector CamTarget = ArmOrigin
			- SmoothedSecondaryRotation.Vector() * SmoothedArmLength
			+ FRotationMatrix(SmoothedSecondaryRotation).TransformVector(SocketOffset);

		// ===== Collision (done in the receiver's world, like a spring arm) =====
		FVector CamLoc = CamTarget;
		if (bDoCollisionTest && World && SmoothedArmLength > KINDA_SMALL_NUMBER)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DynamicSplitScreenSecondaryArm), false);
			Params.AddIgnoredActor(RemotePawn);
			if (IsValid(LocalPawn))
			{
				Params.AddIgnoredActor(LocalPawn);
			}

			FHitResult Hit;
			if (World->SweepSingleByChannel(Hit, ArmOrigin, CamTarget, FQuat::Identity,
				ProbeChannel, FCollisionShape::MakeSphere(ProbeSize), Params))
			{
				CamLoc = Hit.Location;
			}
		}

		SmoothedSecondaryLocation = CamLoc;
	}

#if !UE_BUILD_SHIPPING
	if (World && CVarDynamicSplitScreenDebugDraw.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(World, SmoothedSecondaryLocation, 25.f, 12, FColor::Yellow, false, 0.f, 0, 0.5f);
		DrawDebugLine(World, SmoothedSecondaryLocation, SmoothedSecondaryLocation + SmoothedSecondaryRotation.Vector() * 200.f,
			FColor::Red, false, 0.f, 0, 1.f);
	}
#endif

	// The FOV is horizontal for the remote camera component's aspect ratio (16:9 by default),
	// the same value the main view uses, so both halves match.
	float RemoteAspect = 16.f / 9.f;
	if (const UCameraComponent* RemoteCam = RemotePawn->FindComponentByClass<UCameraComponent>())
	{
		if (RemoteCam->AspectRatio > KINDA_SMALL_NUMBER)
		{
			RemoteAspect = RemoteCam->AspectRatio;
		}
	}

	VC->SetSecondaryView(SmoothedSecondaryLocation, SmoothedSecondaryRotation, SmoothedSecondaryFOV, RemoteAspect);
}

// ── Tick ──

void UDynamicSplitScreenSubsystem::Tick(float DeltaTime)
{
	// A destroyed requester (volume deleted, level unloaded) never calls Release. Without this we'd be stuck in full screen.
	if (FullScreenRequesters.Num() > 0)
	{
		const int32 Removed = FullScreenRequesters.RemoveAll(
			[](const TWeakObjectPtr<const UObject>& P) { return !P.IsValid(); });

		if (Removed > 0 && FullScreenRequesters.Num() == 0)
		{
			TransitionToSplitScreen();
		}
	}

	if (!ResolveViewportClient())
	{
		return;
	}

	if (!FMath::IsNearlyEqual(CurrentAlpha, TargetAlpha))
	{
		const float Step = DeltaTime / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER);
		CurrentAlpha = FMath::FInterpConstantTo(CurrentAlpha, TargetAlpha, 1.f, Step);
		CachedViewportClient->SetFullscreenAlpha(CurrentAlpha);
	}

	PushSecondaryCamera();
}

TStatId UDynamicSplitScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDynamicSplitScreenSubsystem, STATGROUP_Tickables);
}

bool UDynamicSplitScreenSubsystem::IsTickable() const
{
	return bSplitScreenActive && !IsTemplate() && GetGameInstance() != nullptr;
}

// ── Public API ──

void UDynamicSplitScreenSubsystem::EnableSplitScreen()
{
	bSplitScreenActive = true;

	// Engine split screen (one view per LocalPlayer) is not used; we build the ViewFamily ourselves
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameViewportClient* GVC = GI->GetGameViewportClient())
		{
			GVC->SetForceDisableSplitscreen(true);
		}
	}

	if (!ResolveViewportClient())
	{
		UE_LOG(LogDynamicSplitScreen, Warning,
			TEXT("EnableSplitScreen: Game Viewport Client Class is not DynamicSplitScreenViewportClient. Set it in Project Settings > General Settings."));
	}

	UE_LOG(LogDynamicSplitScreen, Log, TEXT("Split screen enabled"));
}

void UDynamicSplitScreenSubsystem::DisableSplitScreen()
{
	bSplitScreenActive = false;
	if (UDynamicSplitScreenViewportClient* VC = CachedViewportClient.Get())
	{
		VC->ClearSecondaryView();
	}
	ResetSecondaryState();
	UE_LOG(LogDynamicSplitScreen, Log, TEXT("Split screen disabled"));
}

void UDynamicSplitScreenSubsystem::TransitionToFullScreen()
{
	TargetAlpha = 1.f;
}

void UDynamicSplitScreenSubsystem::TransitionToSplitScreen()
{
	TargetAlpha = 0.f;
}

bool UDynamicSplitScreenSubsystem::IsInFullScreenMode() const
{
	return TargetAlpha >= 1.f - KINDA_SMALL_NUMBER && CurrentAlpha >= 1.f - KINDA_SMALL_NUMBER;
}

void UDynamicSplitScreenSubsystem::RequestFullScreen(const UObject* Requester)
{
	if (!Requester) return;

	FullScreenRequesters.RemoveAll([](const TWeakObjectPtr<const UObject>& P) { return !P.IsValid(); });
	FullScreenRequesters.AddUnique(TWeakObjectPtr<const UObject>(Requester));

	TransitionToFullScreen();
}

void UDynamicSplitScreenSubsystem::ReleaseFullScreen(const UObject* Requester)
{
	FullScreenRequesters.RemoveAll([Requester](const TWeakObjectPtr<const UObject>& P)
	{
		return !P.IsValid() || P.Get() == Requester;
	});

	if (FullScreenRequesters.Num() == 0)
	{
		TransitionToSplitScreen();
	}
}

bool UDynamicSplitScreenSubsystem::ShouldLocalViewRespondTo(const ACharacter* Character, bool bForEnteringPlayer, int32 FixedPlayerIndex)
{
	if (!IsValid(Character)) return false;

	if (bForEnteringPlayer)
	{
		// A remote player's PlayerController does not exist on clients, and on the host it has no LocalPlayer.
		return Character->IsLocallyControlled();
	}

	const APlayerController* PC = Cast<APlayerController>(Character->GetController());
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP && LP->GetControllerId() == FixedPlayerIndex;
}

void UDynamicSplitScreenSubsystem::SetMainViewOnRight(bool bOnRight)
{
	bMainViewOnRight = bOnRight;
	if (UDynamicSplitScreenViewportClient* VC = CachedViewportClient.Get())
	{
		VC->SetSwapLeftRight(bMainViewOnRight);
	}
}

bool UDynamicSplitScreenSubsystem::TryGetVisibleSecondaryView(FVector& OutCameraLocation, APawn*& OutTarget) const
{
	if (!bSplitScreenActive || !bHasSmoothedSecondary || IsInFullScreenMode()) return false;

	APawn* Pawn = LastGoodRemotePawn.Get();
	if (!IsValid(Pawn)) return false;

	OutCameraLocation = SmoothedSecondaryLocation;
	OutTarget = Pawn;
	return true;
}

void UDynamicSplitScreenSubsystem::StartScreenFade(float TargetFadeAlpha, float Duration)
{
	UGameInstance* GI = GetGameInstance();
	if (UDynamicSplitScreenViewportClient* VC = GI ? Cast<UDynamicSplitScreenViewportClient>(GI->GetGameViewportClient()) : nullptr)
	{
		VC->StartFade(TargetFadeAlpha, Duration);
	}
}
