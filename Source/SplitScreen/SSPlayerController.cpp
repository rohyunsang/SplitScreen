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
#include "Net/UnrealNetwork.h"  // 꼭 필요

void ASSPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASSPlayerController, RepCam);   // RepCam 등록
}

void ASSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority()) return;

    if (!bIsDummyController)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Player Controller Started - IsLocalController: %s"),
            IsLocalController() ? TEXT("true") : TEXT("false"));

        // 클라이언트에서 로컬 컨트롤러인 경우 스플릿 스크린 설정
        if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client detected - Setting up split screen"));

            if (bClientSplitScreenSetupComplete)
            {
                UE_LOG(LogTemp, Warning, TEXT("SS Client split screen already setup, skipping"));
                return;
            }

            // GameInstance에서 스플릿 스크린 활성화
            if (USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance()))
            {
                SSGI->EnableSplitScreen();
            }

            // 더미 컨트롤러 생성 및 스펙테이터 설정
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

    // 더미 로컬 플레이어 생성
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

    // 기존 더미 컨트롤러 찾기
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

    // 새 더미 컨트롤러 생성
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
    {
        ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
        if (SecondLocalPlayer)
        {
            if (!SecondLocalPlayer->PlayerController)
            {
                // 새 컨트롤러 생성
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
                // 기존 컨트롤러 사용
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

    // 더미 컨트롤러를 스펙테이터 모드로 전환
    ClientDummyController->ChangeState(NAME_Spectating);

    // CameraViewProxy 찾기 또는 생성
    ASSCameraViewProxy* CameraProxy = nullptr;

    // 기존 CameraProxy 찾기
    for (TActorIterator<ASSCameraViewProxy> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
    {
        CameraProxy = *ActorIterator;
        break;
    }

    // CameraProxy를 뷰 타겟으로 설정
    if (CameraProxy)
    {
        ClientDummyController->SetViewTargetWithBlend(CameraProxy, SpectatorBlendTime);
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy controller now following camera proxy"));
    }
    else
    {
        // 기존 방식으로 fallback
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

            // 재시도 로직...
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

    // 서버에서는 PlayerController 이터레이터로 원격 플레이어 찾기
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
        // 클라이언트에서는 GameState를 통해 다른 플레이어 찾기
        AGameStateBase* GameState = GetWorld()->GetGameState();
        if (GameState)
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client checking GameState PlayerArray, count: %d"), GameState->PlayerArray.Num());

            for (APlayerState* PS : GameState->PlayerArray)
            {
                if (PS && PS != GetPlayerState<APlayerState>() && PS->GetPawn())
                {
                    // 추가 검증: 자기 자신이 아닌지 확인
                    if (PS->GetPawn() != GetPawn())
                    {
                        UE_LOG(LogTemp, Warning, TEXT("SS Client found remote player pawn via GameState: %s"), *PS->GetPawn()->GetName());
                        return PS->GetPawn();
                    }
                }
            }
        }

        // GameState가 없거나 실패한 경우 fallback으로 non-dummy 컨트롤러 찾기
        UE_LOG(LogTemp, Warning, TEXT("SS Client fallback: checking PlayerController iterator"));

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ASSPlayerController* SSPC = Cast<ASSPlayerController>(It->Get());
            if (SSPC && SSPC != this && !SSPC->bIsDummyController && SSPC->GetPawn())
            {
                // 추가 검증: 자기 자신이 아닌지 확인
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

    // 더미 컨트롤러에서는 입력 처리 안함
    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy Controller - Skipping input setup"));
        return;
    }
}

void ASSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 더미 컨트롤러는 네트워크 동기화 안함
    if (bIsDummyController) return;

    // 로컬 플레이어만 위치 정보를 서버로 전송
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

    // 스펙테이터 타겟이 유효한지 주기적으로 체크 (더미 컨트롤러가 아닌 경우에만)
    if (ClientDummyController && CurrentSpectatorTarget)
    {
        // 타겟이 삭제되었거나 무효해진 경우 재탐색
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
    // 서버에서 클라이언트들의 스펙테이터에게 타겟 업데이트 알림
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASSPlayerController* OtherPC = Cast<ASSPlayerController>(It->Get());
        if (OtherPC && OtherPC != this && !OtherPC->IsLocalController())
        {
            // 다른 클라이언트의 더미 컨트롤러에게 이 플레이어를 타겟으로 설정하도록 지시
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
    // 스펙테이터 모드에서는 자동으로 동기화되므로 별도 처리 불필요
    UE_LOG(LogTemp, VeryVerbose, TEXT("SS Received remote player location (spectator mode handles sync automatically)"));
}
void ASSPlayerController::OnRep_RepCam()
{
    // 클라에서 RepCam 갱신 시 반영할 일 처리
    // 예: 더미 카메라/컨트롤러에 적용 or 로그
    // UE_LOG(LogTemp, Verbose, TEXT("OnRep_RepCam: Loc=%s Rot=%s FOV=%.2f"),
    //     *RepCam.Location.ToString(), *RepCam.Rotation.ToString(), RepCam.FOV);
}