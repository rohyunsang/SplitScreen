// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicSplitScreenGameMode.h"
#include "DynamicSplitScreen.h"
#include "Actors/DynamicSplitScreenCameraProxy.h"
#include "Controllers/DynamicSplitScreenPlayerController.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ADynamicSplitScreenGameMode::ADynamicSplitScreenGameMode()
{
	PlayerControllerClass = ADynamicSplitScreenPlayerController::StaticClass();
	CameraProxyClass = ADynamicSplitScreenCameraProxy::StaticClass();
}

void ADynamicSplitScreenGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_Standalone)
	{
		UE_LOG(LogDynamicSplitScreen, Warning,
			TEXT("Split screen needs two players over the network. In the editor use Play > Net Mode: Play As Listen Server, Number of Players: 2."));

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 15.f, FColor::Yellow,
				TEXT("Dynamic Split Screen: run with Net Mode 'Play As Listen Server' and Number of Players = 2 to see split screen."));
		}
#endif
	}
}

void ADynamicSplitScreenGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	CreateCameraProxiesForPlayer(NewPlayer);
	TryEnableSplitScreen();
}

void ADynamicSplitScreenGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		if (TObjectPtr<ADynamicSplitScreenCameraProxy>* Found = ClientCameraProxies.Find(PC))
		{
			if (IsValid(*Found))
			{
				(*Found)->Destroy();
			}
			ClientCameraProxies.Remove(PC);
		}
	}

	Super::Logout(Exiting);
}

void ADynamicSplitScreenGameMode::CreateCameraProxiesForPlayer(APlayerController* NewPlayer)
{
	if (!NewPlayer) return;

	UClass* ProxyClass = CameraProxyClass ? CameraProxyClass.Get() : ADynamicSplitScreenCameraProxy::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (!NewPlayer->IsLocalController())
	{
		// Remote client: it sends its camera up through RPC and the proxy replicates it to everyone
		SpawnParams.Owner = NewPlayer;
		ADynamicSplitScreenCameraProxy* ClientProxy = GetWorld()->SpawnActor<ADynamicSplitScreenCameraProxy>(ProxyClass, FTransform::Identity, SpawnParams);
		if (ClientProxy)
		{
			ClientProxy->SetIsServerProxy(false);
			ClientCameraProxies.Add(NewPlayer, ClientProxy);
		}
	}
	else if (!ServerCameraProxy)
	{
		// Listen server's local player: the host fills this proxy with its own camera
		ServerCameraProxy = GetWorld()->SpawnActor<ADynamicSplitScreenCameraProxy>(ProxyClass, FTransform::Identity, SpawnParams);
		if (ServerCameraProxy)
		{
			ServerCameraProxy->SetIsServerProxy(true);
			ServerCameraProxy->SetSourcePC(NewPlayer);
		}
	}
}

void ADynamicSplitScreenGameMode::TryEnableSplitScreen()
{
	if (!bAutoEnableSplitScreen) return;
	if (GetNetMode() != NM_ListenServer) return;
	if (GetNumPlayers() < 2) return;

	if (UDynamicSplitScreenSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UDynamicSplitScreenSubsystem>())
	{
		Subsystem->EnableSplitScreen();
	}
}
