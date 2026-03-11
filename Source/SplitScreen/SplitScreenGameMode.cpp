// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplitScreenGameMode.h"
#include "SplitScreenCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "DynamicSplitScreen/Public/Subsystem/DynamicSplitScreenSubsystem.h"
#include "SplitScreenPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "SplitScreenSpectatorPawn.h"
#include "TimerManager.h"

ASplitScreenGameMode::ASplitScreenGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	PlayerControllerClass = ASplitScreenPlayerController::StaticClass();
}

void ASplitScreenGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 플러그인에 정의된 DynamicSplitScreenSubsystem을 가져와 화면 분할 기능을 활성화합니다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		// 분할 화면이 실제로 그려지려면 로컬 플레이어가 최소 2명이어야 합니다.
		// WindUp 방식 도입: 게임 메카닉에 의해 캐릭터가 불필요하게 스폰되는 것을 방지
		if (GameInstance->GetNumLocalPlayers() < 2)
		{
			FString OutError;
			ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(1, OutError, false);
			if (DummyLocalPlayer)
			{
				DummyPlayerController = GetWorld()->SpawnActor<ASplitScreenPlayerController>();
				if (ASplitScreenPlayerController* SSC = Cast<ASplitScreenPlayerController>(DummyPlayerController))
				{
					SSC->SetAsDummyController(true);
					SSC->SetPlayer(DummyLocalPlayer);
				}
			}
		}

		if (UDynamicSplitScreenSubsystem* SplitSubsystem = GameInstance->GetSubsystem<UDynamicSplitScreenSubsystem>())
		{
			SplitSubsystem->EnableSplitScreen();
		}
	}
}

void ASplitScreenGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer && !NewPlayer->IsLocalController())
	{
		AttachDummySpectatorToClient(NewPlayer);
	}
}

void ASplitScreenGameMode::AttachDummySpectatorToClient(APlayerController* RemoteClient)
{
	if (!RemoteClient || !RemoteClient->GetPawn())
	{
		return;
	}

	CachedRemoteClient = RemoteClient;
	APawn* ClientPawn = RemoteClient->GetPawn();
	USkeletalMeshComponent* Mesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>();
	
	if (!Mesh) return;

	if (!DummySpectatorPawn)
	{
		DummySpectatorPawn = GetWorld()->SpawnActor<ASplitScreenSpectatorPawn>(
			ASplitScreenSpectatorPawn::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator
		);
	}

	if (DummySpectatorPawn)
	{
		FName AttachSocketName = TEXT("camera_socket");
		if (!Mesh->DoesSocketExist(AttachSocketName))
		{
			// 소켓이 없으면 그냥 Root에 부착
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
				&ASplitScreenGameMode::SyncDummyWithRemoteClient,
				0.016f,
				true
			);
		}
	}
}

void ASplitScreenGameMode::SyncDummyWithRemoteClient()
{
	if (!DummySpectatorPawn || !DummyPlayerController || !CachedRemoteClient) return;

	APawn* ClientPawn = CachedRemoteClient->GetPawn();
	if (!ClientPawn) return;

	// 이미 클라이언트 폰에 부착(Attach)되어 있으므로 위치는 자동으로 제어됩니다.
	// 수동 위치 보간 코드를 제거하여 카메라 떨림 현상을 해결합니다.

	// 클라 카메라 회전 대신, 클라의 컨트롤 로테이션을 가져옴
	FRotator TargetRot = CachedRemoteClient->GetControlRotation();
	FRotator CurrentRot = DummyPlayerController->GetControlRotation();

	FRotator NewRot = FMath::RInterpTo(
		CurrentRot,
		TargetRot,
		GetWorld()->GetDeltaSeconds(),
		20.f
	);

	DummyPlayerController->SetControlRotation(NewRot);
}

