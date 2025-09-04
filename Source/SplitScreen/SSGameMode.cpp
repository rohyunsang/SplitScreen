// Fill out your copyright notice in the Description page of Project Settings.


#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h" 

ASSGameMode::ASSGameMode()
{
    // 기본 클래스들 설정
    PlayerControllerClass = ASSPlayerController::StaticClass();

    // set default pawn class to our Blueprinted character
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (PlayerPawnBPClass.Class != NULL)
    {
        DefaultPawnClass = PlayerPawnBPClass.Class;
    }

    // 더미 스펙테이터 폰 클래스 설정
    DummySpectatorPawnClass = ASSDummySpectatorPawn::StaticClass();
}

void ASSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoEnableSplitScreen)
    {
        USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance());
        if (SSGI)
        {
            SSGI->EnableSplitScreen();
        }
    }
}

void ASSGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    ConnectedPlayers.AddUnique(NewPlayer);

    UE_LOG(LogTemp, Warning, TEXT("SS Player Connected. Total Players: %d"), ConnectedPlayers.Num());

    // 두 번째 플레이어가 접속하면 스플릿 스크린 설정
    if (ConnectedPlayers.Num() >= 2 && bAutoEnableSplitScreen)
    {
        SetupOnlineSplitScreen();
    }
}

void ASSGameMode::Logout(AController* Exiting)
{
    APlayerController* PC = Cast<APlayerController>(Exiting);
    if (PC)
    {
        ConnectedPlayers.Remove(PC);
    }

    Super::Logout(Exiting);
}

void ASSGameMode::SetupOnlineSplitScreen()
{
    USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance());
    if (!SSGI || !SSGI->IsSplitScreenEnabled()) return;

    // 이미 설정되어 있으면 리턴
    if (DummyPlayerController) return;

    CreateDummyLocalPlayer();
    UpdateSplitScreenLayout();

    // 주기적으로 더미 플레이어 위치 동기화
    GetWorldTimerManager().SetTimer(
        SyncTimerHandle,
        this,
        &ASSGameMode::SyncDummyPlayerWithRemotePlayer,
        0.1f, // 10fps로 동기화
        true
    );
}

void ASSGameMode::CreateDummyLocalPlayer()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // 현재 로컬 플레이어 수 확인
    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Already have 2+ local players"));
        return;
    }

    // 더미 로컬 플레이어 생성 // 수정 (언리얼 5 방식)  
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, true);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player"));
        return;
    }

    // 더미 스펙테이터 폰 생성
    FVector SpawnLocation = FVector(0, 0, 200);
    FRotator SpawnRotation = FRotator::ZeroRotator;

    DummySpectatorPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
        DummySpectatorPawnClass,
        SpawnLocation,
        SpawnRotation
    );

    if (!DummySpectatorPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to spawn dummy spectator pawn"));
        return;
    }

    // 더미 플레이어 컨트롤러 생성
    DummyPlayerController = GetWorld()->SpawnActor<ASSPlayerController>();
    if (DummyPlayerController)
    {
        DummyPlayerController->SetPlayer(DummyLocalPlayer);
        DummyPlayerController->Possess(DummySpectatorPawn);

        UE_LOG(LogTemp, Warning, TEXT("SS Dummy Local Player Created Successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy player controller"));
    }
}

void ASSGameMode::SyncDummyPlayerWithRemotePlayer()
{
    if (!DummySpectatorPawn || ConnectedPlayers.Num() < 2) return;

    // 두 번째 플레이어 (원격 플레이어)의 위치를 더미 플레이어에 동기화
    APlayerController* RemotePlayer = nullptr;

    // 첫 번째가 아닌 플레이어 찾기
    for (int32 i = 1; i < ConnectedPlayers.Num(); i++)
    {
        if (ConnectedPlayers[i] && ConnectedPlayers[i] != DummyPlayerController)
        {
            RemotePlayer = ConnectedPlayers[i];
            break;
        }
    }

    if (!RemotePlayer || !RemotePlayer->GetPawn()) return;

    APawn* RemotePawn = RemotePlayer->GetPawn();
    DummySpectatorPawn->SyncWithRemotePlayer(
        RemotePawn->GetActorLocation(),
        RemotePawn->GetActorRotation()
    );
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // 뷰포트 레이아웃은 엔진이 자동으로 처리
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}
