// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicSplitScreenGameMode.h"
#include "Controllers/DynamicSplitScreenSpectatorPawn.h"
#include "Controllers/DynamicSplitScreenPlayerController.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"

ADynamicSplitScreenGameMode::ADynamicSplitScreenGameMode()
{
	PlayerControllerClass = ADynamicSplitScreenPlayerController::StaticClass();
	DummySpectatorPawnClass = ADynamicSplitScreenSpectatorPawn::StaticClass();
	DummyPlayerControllerClass = ADynamicSplitScreenPlayerController::StaticClass();
}

void ADynamicSplitScreenGameMode::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	if (GameInstance->GetNumLocalPlayers() < 2)
	{
		FString OutError;
		ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(1, OutError, false);
		if (DummyLocalPlayer)
		{
			TSubclassOf<ADynamicSplitScreenPlayerController> ControllerClass = DummyPlayerControllerClass;
			if (!ControllerClass)
			{
				ControllerClass = ADynamicSplitScreenPlayerController::StaticClass();
			}

			DummyPlayerController = GetWorld()->SpawnActor<ADynamicSplitScreenPlayerController>(ControllerClass);
			if (ADynamicSplitScreenPlayerController* DSC = Cast<ADynamicSplitScreenPlayerController>(DummyPlayerController))
			{
				DSC->SetAsDummyController(true);
				DSC->SetPlayer(DummyLocalPlayer);
			}
		}
	}

	if (UDynamicSplitScreenSubsystem* Subsystem = GameInstance->GetSubsystem<UDynamicSplitScreenSubsystem>())
	{
		Subsystem->EnableSplitScreen();
	}
}

void ADynamicSplitScreenGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer && !NewPlayer->IsLocalController())
	{
		AttachDummySpectatorToClient(NewPlayer);
	}
}

void ADynamicSplitScreenGameMode::AttachDummySpectatorToClient(APlayerController* RemoteClient)
{
	if (!RemoteClient || !RemoteClient->GetPawn()) return;

	CachedRemoteClient = RemoteClient;
	APawn* ClientPawn = RemoteClient->GetPawn();
	USkeletalMeshComponent* Mesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>();

	if (!Mesh) return;

	if (!DummySpectatorPawn)
	{
		TSubclassOf<ADynamicSplitScreenSpectatorPawn> SpawnClass = DummySpectatorPawnClass;
		if (!SpawnClass)
		{
			SpawnClass = ADynamicSplitScreenSpectatorPawn::StaticClass();
		}

		DummySpectatorPawn = GetWorld()->SpawnActor<ADynamicSplitScreenSpectatorPawn>(
			SpawnClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator
		);
	}

	if (!DummySpectatorPawn) return;

	FName AttachSocketName = TEXT("camera_socket");
	if (!Mesh->DoesSocketExist(AttachSocketName))
	{
		DummySpectatorPawn->AttachToActor(ClientPawn, FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true);
		DummySpectatorPawn->AttachToComponent(Mesh, AttachRules, AttachSocketName);
	}

	DummySpectatorPawn->SetActorHiddenInGame(true);
	DummySpectatorPawn->SetActorEnableCollision(false);

	if (DummyPlayerController && !DummySpectatorPawn->GetController())
	{
		DummyPlayerController->Possess(DummySpectatorPawn);
	}

	if (!GetWorldTimerManager().IsTimerActive(SyncTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			SyncTimerHandle,
			this,
			&ADynamicSplitScreenGameMode::SyncDummyWithRemoteClient,
			0.016f,
			true
		);
	}
}

void ADynamicSplitScreenGameMode::SyncDummyWithRemoteClient()
{
	if (!DummySpectatorPawn || !DummyPlayerController || !CachedRemoteClient) return;

	if (!CachedRemoteClient->GetPawn()) return;

	const FRotator TargetRot = CachedRemoteClient->GetControlRotation();
	const FRotator CurrentRot = DummyPlayerController->GetControlRotation();

	const FRotator NewRot = FMath::RInterpTo(
		CurrentRot,
		TargetRot,
		GetWorld()->GetDeltaSeconds(),
		20.f
	);

	DummyPlayerController->SetControlRotation(NewRot);
}
