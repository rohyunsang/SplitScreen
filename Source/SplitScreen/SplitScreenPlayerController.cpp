// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplitScreenPlayerController.h"
#include "DynamicSplitScreen/Public/Subsystem/DynamicSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SplitScreenSpectatorPawn.h"
#include "EngineUtils.h"
#include "SplitScreenCharacter.h"

ASplitScreenPlayerController::ASplitScreenPlayerController()
{
}

void ASplitScreenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bIsDummyController)
	{
		return;
	}

	// 클라이언트에서 로컬 컨트롤러인 경우 스플릿 스크린 설정
	if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
	{
		if (bClientSplitScreenSetupComplete)
		{
			return;
		}

		// GameInstance에서 서브시스템 활성화
		if (UDynamicSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UDynamicSplitScreenSubsystem>())
		{
			CSSplitSubsystem->EnableSplitScreen();
		}

		// 폴링 타이머: 안정적인 설정을 위해 0.5초 대기하며 셋업 (WindUp 방식)
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

void ASplitScreenPlayerController::SetupClientSplitScreen()
{
	if (bClientSplitScreenSetupComplete) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();

	if (CurrentLocalPlayers >= 2)
	{
		bClientSplitScreenSetupComplete = true;
		return;
	}

	// 더미 로컬 플레이어 생성
	FPlatformUserId DummyUserId = FPlatformUserId::CreateFromInternalId(1);
	FString OutError;
	ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

	if (DummyLocalPlayer)
	{
		// 셋업 완료 전 더미 폰 생성 (WindUp 방식)
		CreateClientDummyPawn();

		bClientSplitScreenSetupComplete = true;
	}
}

void ASplitScreenPlayerController::CreateClientDummyPawn()
{
	if (ClientDummyPawn && IsValid(ClientDummyPawn))
	{
		return;
	}

	FVector DummySpawnLocation = FVector(0, 0, 200);
	FRotator DummySpawnRotation = FRotator::ZeroRotator;

	ClientDummyPawn = GetWorld()->SpawnActor<ASplitScreenSpectatorPawn>(
		ASplitScreenSpectatorPawn::StaticClass(),
		DummySpawnLocation,
		DummySpawnRotation
	);

	if (ClientDummyPawn)
	{
		ClientDummyPawn->Rename(TEXT("ClientSpectatorPawn"));

		// 1) 기존 더미 컨트롤러 찾기
		APlayerController* ExistingDummyController = nullptr;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->GetLocalPlayer() && PC->GetLocalPlayer()->GetControllerId() == 1) // 추론: id 1인 두번째 로컬플레이어
			{
				ExistingDummyController = PC;
				break;
			}
		}

		APlayerController* DummyController = ExistingDummyController;

		if (!DummyController)
		{
			UGameInstance* GameInstance = GetGameInstance();
			if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
			{
				ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
				if (SecondLocalPlayer && !SecondLocalPlayer->PlayerController)
				{
					DummyController = GetWorld()->SpawnActor<ASplitScreenPlayerController>();
					if (ASplitScreenPlayerController* SSC = Cast<ASplitScreenPlayerController>(DummyController))
					{
						SSC->SetAsDummyController(true);
					}
				}
				else if (SecondLocalPlayer && SecondLocalPlayer->PlayerController)
				{
					DummyController = SecondLocalPlayer->PlayerController;
					if (ASplitScreenPlayerController* SSC = Cast<ASplitScreenPlayerController>(DummyController))
					{
						SSC->SetAsDummyController(true);
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
	}

	AttachDummySpectatorToRemoteCharacter(ClientDummyPawn);
}

void ASplitScreenPlayerController::StartClientDummySync(ASplitScreenSpectatorPawn* DummyPawn)
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

void ASplitScreenPlayerController::AttachDummySpectatorToRemoteCharacter(ASplitScreenSpectatorPawn* DummyPawn)
{
	if (!DummyPawn) return;

	ASplitScreenCharacter* RemoteChar = nullptr;
	for (TActorIterator<ASplitScreenCharacter> It(GetWorld()); It; ++It)
	{
		ASplitScreenCharacter* Char = *It;
		if (!Char || Char->IsLocallyControlled()) continue;
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
		// 카메라 소켓의 위치만 따라가고 회전은 컨트롤 로테이션을 따르게 하기 위해 회전은 KeepWorld 보존
		FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true);
		DummyPawn->AttachToComponent(Mesh, AttachRules, AttachSocketName);
	}

	DummyPawn->SetActorHiddenInGame(true);
	DummyPawn->SetActorEnableCollision(false);
}

void ASplitScreenPlayerController::SyncClientDummyWithRemotePlayer(ASplitScreenSpectatorPawn* DummyPawn)
{
	// 서버 프록시 동기화 로직은 삭제되었으므로, 일단 원격 캐릭터를 따라가도록만 설정.
	// 완전한 시점 동기화를 위해선 서버 측 회전값을 리플리케이트 받아야 하지만, 
	// 일단 틱마다 원격 캐릭터의 회전과 위치를 따라가도록 기본 동기화 함.
	if (!DummyPawn) return;

	ASplitScreenCharacter* TargetCharacter = CachedRemoteCharacter.Get();
	if (!TargetCharacter)
	{
		for (TActorIterator<ASplitScreenCharacter> It(GetWorld()); It; ++It)
		{
			if (*It && !(*It)->IsLocallyControlled())
			{
				TargetCharacter = *It;
				CachedRemoteCharacter = TargetCharacter;
				break;
			}
		}
		if (!TargetCharacter) return;
	}

	// 이미 Attach 되어 있으므로 수동으로 위치를 보간(SetActorLocation)하지 않습니다. (이전 코드가 떨림 유발)
	// 회전만 동기화하여 시점을 맞춰줍니다.

	if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
	{
		// 서버 카메라 회전 데이터를 대신해 타겟 캐릭터의 리플리케이트된 카메라 회전을 따라감
		const FRotator CurrentRot = DummyController->GetControlRotation();
		const FRotator TargetRot = TargetCharacter->GetReplicatedCameraRotation(); 
		const float RotInterpSpeed = 45.f;
		const FRotator SmoothedRot = FMath::RInterpTo(CurrentRot, TargetRot, GetWorld()->GetDeltaSeconds(), RotInterpSpeed);

		DummyController->SetControlRotation(SmoothedRot);
	}
}

void ASplitScreenPlayerController::SetAsDummyController(bool bDummy)
{
	bIsDummyController = bDummy;
}

