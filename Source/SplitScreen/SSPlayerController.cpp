// Fill out your copyright notice in the Description page of Project Settings.

#include "SSPlayerController.h"
#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMisc.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"  // �� �ʿ�

void ASSPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASSPlayerController, RepCam);   // RepCam ���
}

void ASSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority()) return;

    if (!bIsDummyController)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Player Controller Started - IsLocalController: %s"),
            IsLocalController() ? TEXT("true") : TEXT("false"));

        // Ŭ���̾�Ʈ���� ���� ��Ʈ�ѷ��� ��� ���ø� ��ũ�� ����
        if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client detected - Setting up split screen"));

            if (bClientSplitScreenSetupComplete)
            {
                UE_LOG(LogTemp, Warning, TEXT("SS Client split screen already setup, skipping"));
                return;
            }

            // GameInstance���� ���ø� ��ũ�� Ȱ��ȭ
            if (USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance()))
            {
                SSGI->EnableSplitScreen();
            }

            // ���� ��Ʈ�ѷ� ���� �� ���������� ����
            FTimerHandle ClientSetupHandle;
            GetWorldTimerManager().SetTimer(
                ClientSetupHandle,
                [this]()
                {
                    if (!bClientSplitScreenSetupComplete)
                    {
                        SetupClientSplitScreen();
                    }
                },
                2.0f,
                false
            );
        }
    }
}

void ASSPlayerController::SetupClientSplitScreen()
{
    if (bClientSplitScreenSetupComplete)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client split screen setup already complete"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up client split screen with spectator mode"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();
    UE_LOG(LogTemp, Warning, TEXT("SS Client current local players: %d"), CurrentLocalPlayers);

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client already has 2+ local players"));

        if (!ClientDummyController)
        {
            CreateClientDummyController();
        }

        bClientSplitScreenSetupComplete = true;
        return;
    }

    // ���� ���� �÷��̾� ����
    FPlatformUserId DummyUserId = FPlatformUserId::CreateFromInternalId(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy local player created successfully"));
        CreateClientDummyController();
        bClientSplitScreenSetupComplete = true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Client failed to create dummy local player: %s"), *OutError);
    }
}

void ASSPlayerController::CreateClientDummyController()
{
    UE_LOG(LogTemp, Warning, TEXT("SS Creating client dummy controller for spectator mode"));

    // ���� ���� ��Ʈ�ѷ� ã��
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASSPlayerController* PC = Cast<ASSPlayerController>(It->Get());
        if (PC && PC->bIsDummyController && PC != this)
        {
            ClientDummyController = PC;
            UE_LOG(LogTemp, Warning, TEXT("SS Found existing dummy controller: %s"), *PC->GetName());
            SetupSpectatorForDummyController();
            return;
        }
    }

    // �� ���� ��Ʈ�ѷ� ����
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
    {
        ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
        if (SecondLocalPlayer)
        {
            if (!SecondLocalPlayer->PlayerController)
            {
                // �� ��Ʈ�ѷ� ����
                ClientDummyController = GetWorld()->SpawnActor<ASSPlayerController>();
                if (ClientDummyController)
                {
                    Cast<ASSPlayerController>(ClientDummyController)->SetAsDummyController(true);
                    ClientDummyController->SetPlayer(SecondLocalPlayer);
                    UE_LOG(LogTemp, Warning, TEXT("SS New dummy controller created: %s"), *ClientDummyController->GetName());
                }
            }
            else
            {
                // ���� ��Ʈ�ѷ� ���
                ClientDummyController = SecondLocalPlayer->PlayerController;
                if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(ClientDummyController))
                {
                    SSPC->SetAsDummyController(true);
                }
            }

            SetupSpectatorForDummyController();
        }
    }
}

void ASSPlayerController::SetupSpectatorForDummyController()
{
    if (!ClientDummyController)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Cannot setup spectator - dummy controller is null"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up spectator mode for dummy controller"));

    // ���� ��Ʈ�ѷ��� ���������� ���� ��ȯ
    ClientDummyController->ChangeState(NAME_Spectating);

    // CameraViewProxy ã�� �Ǵ� ����
    ASSCameraViewProxy* CameraProxy = nullptr;

    // ���� CameraProxy ã��
    for (TActorIterator<ASSCameraViewProxy> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
    {
        CameraProxy = *ActorIterator;
        break;
    }

    // CameraProxy�� �� Ÿ������ ����
    if (CameraProxy)
    {
        ClientDummyController->SetViewTargetWithBlend(CameraProxy, SpectatorBlendTime);
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy controller now following camera proxy"));
    }
    else
    {
        // ���� ������� fallback
        APawn* RemotePlayerPawn = FindRemotePlayerPawn();
        if (RemotePlayerPawn)
        {
            ClientDummyController->SetViewTargetWithBlend(RemotePlayerPawn, SpectatorBlendTime);
            CurrentSpectatorTarget = RemotePlayerPawn;
            UE_LOG(LogTemp, Warning, TEXT("SS Dummy controller now spectating: %s"), *RemotePlayerPawn->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS No camera proxy or remote player found, will retry"));

            // ��õ� ����...
            FTimerHandle RetryHandle;
            GetWorldTimerManager().SetTimer(
                RetryHandle,
                [this]()
                {
                    SetupSpectatorForDummyController();
                },
                1.0f,
                false
            );
        }
    }
}

void ASSPlayerController::SetupSpectatorMode(APlayerController* TargetPC)
{
    if (!TargetPC || !TargetPC->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS SetupSpectatorMode: Invalid target"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up spectator mode targeting: %s"), *TargetPC->GetName());

    ChangeState(NAME_Spectating);
    SetViewTargetWithBlend(TargetPC->GetPawn(), SpectatorBlendTime);
    CurrentSpectatorTarget = TargetPC->GetPawn();
}

void ASSPlayerController::SetSpectatorTarget(APawn* TargetPawn)
{
    if (!TargetPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS SetSpectatorTarget: Invalid target pawn"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting spectator target to: %s"), *TargetPawn->GetName());

    SetViewTargetWithBlend(TargetPawn, SpectatorBlendTime);
    CurrentSpectatorTarget = TargetPawn;
}

void ASSPlayerController::ClientSetSpectatorTarget_Implementation(APawn* TargetPawn)
{
    if (bIsDummyController)
    {
        SetSpectatorTarget(TargetPawn);
    }
}

APawn* ASSPlayerController::FindRemotePlayerPawn()
{
    UE_LOG(LogTemp, Warning, TEXT("SS FindRemotePlayerPawn called - HasAuthority: %s"), HasAuthority() ? TEXT("Server") : TEXT("Client"));

    // ���������� PlayerController ���ͷ����ͷ� ���� �÷��̾� ã��
    if (HasAuthority())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (PC && PC != this && PC->GetPawn() && !PC->IsLocalController())
            {
                UE_LOG(LogTemp, Warning, TEXT("SS Server found remote player pawn: %s"), *PC->GetPawn()->GetName());
                return PC->GetPawn();
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No remote player found"));
    }
    else
    {
        // Ŭ���̾�Ʈ������ GameState�� ���� �ٸ� �÷��̾� ã��
        AGameStateBase* GameState = GetWorld()->GetGameState();
        if (GameState)
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client checking GameState PlayerArray, count: %d"), GameState->PlayerArray.Num());

            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS != GetPlayerState<APlayerState>() && PS->GetPawn())
                {
                    // �߰� ����: �ڱ� �ڽ��� �ƴ��� Ȯ��
                    if (PS->GetPawn() != GetPawn())
                    {
                        UE_LOG(LogTemp, Warning, TEXT("SS Client found remote player pawn via GameState: %s"), *PS->GetPawn()->GetName());
                        return PS->GetPawn();
                    }
                }
            }
        }

        // GameState�� ���ų� ������ ��� fallback���� non-dummy ��Ʈ�ѷ� ã��
        UE_LOG(LogTemp, Warning, TEXT("SS Client fallback: checking PlayerController iterator"));

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ASSPlayerController* SSPC = Cast<ASSPlayerController>(It->Get());
            if (SSPC && SSPC != this && !SSPC->bIsDummyController && SSPC->GetPawn())
            {
                // �߰� ����: �ڱ� �ڽ��� �ƴ��� Ȯ��
                if (SSPC->GetPawn() != GetPawn())
                {
                    UE_LOG(LogTemp, Warning, TEXT("SS Client found non-dummy player pawn (fallback): %s"), *SSPC->GetPawn()->GetName());
                    return SSPC->GetPawn();
                }
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("SS Client: No remote player found"));
    }

    return nullptr;
}

APlayerController* ASSPlayerController::FindRemotePlayerController()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASSPlayerController* SSPC = Cast<ASSPlayerController>(It->Get());
        if (SSPC && SSPC != this && !SSPC->bIsDummyController)
        {
            return SSPC;
        }
    }
    return nullptr;
}

void ASSPlayerController::SetAsDummyController(bool bDummy)
{
    bIsDummyController = bDummy;
    UE_LOG(LogTemp, Log, TEXT("SS Controller %s set as dummy: %s"),
        *GetName(), bDummy ? TEXT("Yes") : TEXT("No"));
}

void ASSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // ���� ��Ʈ�ѷ������� �Է� ó�� ����
    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy Controller - Skipping input setup"));
        return;
    }
}

void ASSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ���� ��Ʈ�ѷ��� ��Ʈ��ũ ����ȭ ����
    if (bIsDummyController) return;

    // ���� �÷��̾ ��ġ ������ ������ ����
    if (IsLocalController() && GetPawn())
    {
        TimeSinceLastUpdate += DeltaTime;
        if (TimeSinceLastUpdate >= LocationUpdateInterval)
        {
            FVector PawnLocation = GetPawn()->GetActorLocation();
            FRotator PawnRotation = GetPawn()->GetActorRotation();
            ServerUpdatePlayerLocation(PawnLocation, PawnRotation);
            TimeSinceLastUpdate = 0.0f;
        }
    }

    // ���������� Ÿ���� ��ȿ���� �ֱ������� üũ (���� ��Ʈ�ѷ��� �ƴ� ��쿡��)
    if (ClientDummyController && CurrentSpectatorTarget)
    {
        // Ÿ���� �����Ǿ��ų� ��ȿ���� ��� ��Ž��
        if (!IsValid(CurrentSpectatorTarget))
        {
            APawn* NewTarget = FindRemotePlayerPawn();
            if (NewTarget && NewTarget != CurrentSpectatorTarget)
            {
                ClientDummyController->SetViewTargetWithBlend(NewTarget, SpectatorBlendTime);
                CurrentSpectatorTarget = NewTarget;
                UE_LOG(LogTemp, Warning, TEXT("SS Spectator target updated to: %s"), *NewTarget->GetName());
            }
        }
    }
}

void ASSPlayerController::ServerUpdatePlayerLocation_Implementation(FVector Location, FRotator Rotation)
{
    // �������� Ŭ���̾�Ʈ���� ���������Ϳ��� Ÿ�� ������Ʈ �˸�
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASSPlayerController* OtherPC = Cast<ASSPlayerController>(It->Get());
        if (OtherPC && OtherPC != this && !OtherPC->IsLocalController())
        {
            // �ٸ� Ŭ���̾�Ʈ�� ���� ��Ʈ�ѷ����� �� �÷��̾ Ÿ������ �����ϵ��� ����
            OtherPC->ClientSetSpectatorTarget(GetPawn());
        }
    }
}

bool ASSPlayerController::ServerUpdatePlayerLocation_Validate(FVector Location, FRotator Rotation)
{
    return true;
}

void ASSPlayerController::ClientReceiveRemotePlayerLocation_Implementation(FVector Location, FRotator Rotation)
{
    // ���������� ��忡���� �ڵ����� ����ȭ�ǹǷ� ���� ó�� ���ʿ�
    UE_LOG(LogTemp, VeryVerbose, TEXT("SS Received remote player location (spectator mode handles sync automatically)"));
}
void ASSPlayerController::OnRep_RepCam()
{
    // Ŭ�󿡼� RepCam ���� �� �ݿ��� �� ó��
    // ��: ���� ī�޶�/��Ʈ�ѷ��� ���� or �α�
    // UE_LOG(LogTemp, Verbose, TEXT("OnRep_RepCam: Loc=%s Rot=%s FOV=%.2f"),
    //     *RepCam.Location.ToString(), *RepCam.Rotation.ToString(), RepCam.FOV);
}