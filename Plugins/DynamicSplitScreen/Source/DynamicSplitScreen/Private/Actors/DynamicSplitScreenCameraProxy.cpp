// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DynamicSplitScreenCameraProxy.h"
#include "DynamicSplitScreen.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

ADynamicSplitScreenCameraProxy::ADynamicSplitScreenCameraProxy()
{
	PrimaryActorTick.bCanEverTick = true;
	// The PlayerCameraManager cache is updated before PostUpdateWork; sampling here avoids one frame of lag
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(60.f);
	SetReplicateMovement(false);	// only RepCam is replicated, the actor transform is unused
}

void ADynamicSplitScreenCameraProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADynamicSplitScreenCameraProxy, bIsServerProxy);
	DOREPLIFETIME(ADynamicSplitScreenCameraProxy, RepCam);
}

void ADynamicSplitScreenCameraProxy::SetSourcePC(APlayerController* InPC)
{
	if (HasAuthority())
	{
		SourcePC = InPC;
	}
}

FDynamicSplitScreenCameraInfo ADynamicSplitScreenCameraProxy::CaptureCamera(const APlayerController* PC)
{
	FDynamicSplitScreenCameraInfo Info;
	if (!PC || !PC->PlayerCameraManager)
	{
		return Info;
	}

	const APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	const FMinimalViewInfo POV = CameraManager->GetCameraCacheView();
	Info.Location = POV.Location;
	Info.Rotation = POV.Rotation;
	Info.FOV = POV.FOV;

	const APawn* Pawn = PC->GetPawn();
	Info.bUsesPawnCamera = Pawn && CameraManager->GetViewTarget() == Pawn && CameraManager->PendingViewTarget.Target == nullptr;

	if (Pawn)
	{
		if (const USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>())
		{
			Info.ArmLength = Arm->TargetArmLength;
		}
	}
	return Info;
}

void ADynamicSplitScreenCameraProxy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		// Client proxies are only written by ServerUpdateClientCamera
		if (GetOwner() != nullptr)
		{
			return;
		}

		// Server proxy: replicate the listen server's own camera
		APlayerController* PC = SourcePC.Get();
		if (!PC)
		{
			PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
			SourcePC = PC;
		}

		if (PC && PC->PlayerCameraManager)
		{
			RepCam = CaptureCamera(PC);
		}
		return;
	}

	// Client: only the proxy owned by our local PlayerController sends
	const float SendInterval = 1.f / FMath::Max(SendRate, 1.f);
	ClientSendTimer += DeltaSeconds;
	if (ClientSendTimer < SendInterval)
	{
		return;
	}
	// Fmod instead of reset: no drift, and only one send after a hitch
	ClientSendTimer = FMath::Fmod(ClientSendTimer, SendInterval);

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager || GetOwner() != PC)
	{
		return;
	}

	ServerUpdateClientCamera(CaptureCamera(PC));
}

void ADynamicSplitScreenCameraProxy::ServerUpdateClientCamera_Implementation(const FDynamicSplitScreenCameraInfo& NewCam)
{
	const APlayerController* OwningPC = Cast<APlayerController>(GetOwner());
	const UNetConnection* Connection = GetNetConnection();

	if (OwningPC && Connection && Connection->PlayerController == OwningPC)
	{
		RepCam = NewCam;
	}
	else
	{
		UE_LOG(LogDynamicSplitScreen, Warning, TEXT("CameraProxy: rejected camera update from a non-owning connection"));
	}
}

bool ADynamicSplitScreenCameraProxy::ServerUpdateClientCamera_Validate(const FDynamicSplitScreenCameraInfo& NewCam)
{
	return !NewCam.Location.ContainsNaN() && !NewCam.Rotation.ContainsNaN() && FMath::IsFinite(NewCam.FOV) && FMath::IsFinite(NewCam.ArmLength);
}
