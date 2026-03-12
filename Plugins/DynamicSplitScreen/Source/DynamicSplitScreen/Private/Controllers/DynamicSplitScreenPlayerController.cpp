// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controllers/DynamicSplitScreenPlayerController.h"
#include "Controllers/DynamicSplitScreenSpectatorPawn.h"
#include "Controllers/DynamicSplitScreenCharacter.h"
#include "Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

ADynamicSplitScreenPlayerController::ADynamicSplitScreenPlayerController()
{
	DummySpectatorPawnClass = ADynamicSplitScreenSpectatorPawn::StaticClass();
}

void ADynamicSplitScreenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bIsDummyController)
	{
		return;
	}

	if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
	{
		if (bClientSplitScreenSetupComplete)
		{
			return;
		}

		if (UDynamicSplitScreenSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UDynamicSplitScreenSubsystem>())
		{
			Subsystem->EnableSplitScreen();
		}

		GetWorldTimerManager().SetTimer(
			ClientSetupRetryHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!IsValid(this) || bClientSplitScreenSetupComplete)
				{
					GetWorldTimerManager().ClearTimer(ClientSetupRetryHandle);
					return;
				}
				SetupClientSplitScreen();
				if (bClientSplitScreenSetupComplete)
				{
					GetWorldTimerManager().ClearTimer(ClientSetupRetryHandle);
				}
			}),
			0.5f,
			true
		);
	}
}

void ADynamicSplitScreenPlayerController::SetupClientSplitScreen()
{
	if (bClientSplitScreenSetupComplete) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	if (GameInstance->GetNumLocalPlayers() >= 2)
	{
		bClientSplitScreenSetupComplete = true;
		return;
	}

	FPlatformUserId DummyUserId = FPlatformUserId::CreateFromInternalId(1);
	FString OutError;
	ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

	if (DummyLocalPlayer)
	{
		CreateClientDummyPawn();
		bClientSplitScreenSetupComplete = true;
	}
}

void ADynamicSplitScreenPlayerController::CreateClientDummyPawn()
{
	if (ClientDummyPawn && IsValid(ClientDummyPawn))
	{
		return;
	}

	TSubclassOf<ADynamicSplitScreenSpectatorPawn> PawnClass = DummySpectatorPawnClass;
	if (!PawnClass)
	{
		PawnClass = ADynamicSplitScreenSpectatorPawn::StaticClass();
	}

	ClientDummyPawn = GetWorld()->SpawnActor<ADynamicSplitScreenSpectatorPawn>(
		PawnClass,
		FVector(0, 0, 200),
		FRotator::ZeroRotator
	);

	if (!ClientDummyPawn) return;

	ClientDummyPawn->Rename(TEXT("ClientSpectatorPawn"));

	APlayerController* DummyController = nullptr;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetLocalPlayer() && PC->GetLocalPlayer()->GetControllerId() == 1)
		{
			DummyController = PC;
			break;
		}
	}

	if (!DummyController)
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
		{
			ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
			if (SecondLocalPlayer && !SecondLocalPlayer->PlayerController)
			{
				DummyController = GetWorld()->SpawnActor<ADynamicSplitScreenPlayerController>(GetClass());
				if (ADynamicSplitScreenPlayerController* DSC = Cast<ADynamicSplitScreenPlayerController>(DummyController))
				{
					DSC->SetAsDummyController(true);
				}
			}
			else if (SecondLocalPlayer && SecondLocalPlayer->PlayerController)
			{
				DummyController = SecondLocalPlayer->PlayerController;
				if (ADynamicSplitScreenPlayerController* DSC = Cast<ADynamicSplitScreenPlayerController>(DummyController))
				{
					DSC->SetAsDummyController(true);
				}
			}
		}
	}

	if (DummyController)
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
		{
			ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
			if (SecondLocalPlayer)
			{
				if (!SecondLocalPlayer->PlayerController || SecondLocalPlayer->PlayerController != DummyController)
				{
					DummyController->SetPlayer(SecondLocalPlayer);
				}

				if (!ClientDummyPawn->GetController() || ClientDummyPawn->GetController() != DummyController)
				{
					DummyController->Possess(ClientDummyPawn);
				}
			}
		}
	}

	if (!GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
	{
		StartClientDummySync(ClientDummyPawn);
	}

	AttachDummySpectatorToRemoteCharacter(ClientDummyPawn);
}

void ADynamicSplitScreenPlayerController::StartClientDummySync(ADynamicSplitScreenSpectatorPawn* DummyPawn)
{
	if (!DummyPawn) return;

	GetWorldTimerManager().SetTimer(
		ClientSyncTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, DummyPawn]()
		{
			if (!IsValid(this) || !IsValid(DummyPawn)) return;
			if (GetWorld()->bIsTearingDown) return;

			SyncClientDummyWithRemotePlayer(DummyPawn);
		}),
		0.016f,
		true
	);
}

void ADynamicSplitScreenPlayerController::AttachDummySpectatorToRemoteCharacter(ADynamicSplitScreenSpectatorPawn* DummyPawn)
{
	if (!DummyPawn) return;

	ACharacter* RemoteChar = nullptr;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Char = *It;
		if (!Char || Char->IsLocallyControlled()) continue;
		if (!Char->GetPlayerState()) continue; // PlayerState는 클라이언트에도 복제됨
		RemoteChar = Char;
		break;
	}

	if (!RemoteChar) return;

	USkeletalMeshComponent* Mesh = RemoteChar->GetMesh();
	if (!Mesh) return;

	FName AttachSocketName = TEXT("camera_socket");
	if (!Mesh->DoesSocketExist(AttachSocketName))
	{
		DummyPawn->AttachToActor(RemoteChar, FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true);
		DummyPawn->AttachToComponent(Mesh, AttachRules, AttachSocketName);
	}

	DummyPawn->SetActorHiddenInGame(true);
	DummyPawn->SetActorEnableCollision(false);
}

void ADynamicSplitScreenPlayerController::SyncClientDummyWithRemotePlayer(ADynamicSplitScreenSpectatorPawn* DummyPawn)
{
	if (!DummyPawn) return;

	ACharacter* TargetCharacter = CachedRemoteCharacter.Get();
	if (!TargetCharacter)
	{
		for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
		{
			ACharacter* Char = *It;
			if (Char && !Char->IsLocallyControlled() && Char->GetPlayerState())
			{
				TargetCharacter = Char;
				CachedRemoteCharacter = TargetCharacter;
				break;
			}
		}
		if (!TargetCharacter) return;
	}

	if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
	{
		const FRotator CurrentRot = DummyController->GetControlRotation();
		const FRotator TargetRot = GetRemoteCharacterCameraRotation(TargetCharacter);
		const FRotator SmoothedRot = FMath::RInterpTo(CurrentRot, TargetRot, GetWorld()->GetDeltaSeconds(), 45.f);

		DummyController->SetControlRotation(SmoothedRot);
	}
}

FRotator ADynamicSplitScreenPlayerController::GetRemoteCharacterCameraRotation(ACharacter* RemoteChar) const
{
	if (!RemoteChar) return FRotator::ZeroRotator;
	if (ADynamicSplitScreenCharacter* DSChar = Cast<ADynamicSplitScreenCharacter>(RemoteChar))
	{
		return DSChar->GetReplicatedCameraRotation();
	}
	// 폴백: 플러그인 캐릭터를 상속하지 않은 경우 내장 복제값 사용
	return RemoteChar->GetBaseAimRotation();
}

void ADynamicSplitScreenPlayerController::SetAsDummyController(bool bDummy)
{
	bIsDummyController = bDummy;
}
