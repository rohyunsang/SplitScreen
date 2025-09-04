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
    // �⺻ Ŭ������ ����
    PlayerControllerClass = ASSPlayerController::StaticClass();

    // set default pawn class to our Blueprinted character
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (PlayerPawnBPClass.Class != NULL)
    {
        DefaultPawnClass = PlayerPawnBPClass.Class;
    }

    // ���� ���������� �� Ŭ���� ����
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

    // �� ��° �÷��̾ �����ϸ� ���ø� ��ũ�� ����
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

    // �̹� �����Ǿ� ������ ����
    if (DummyPlayerController) return;

    CreateDummyLocalPlayer();
    UpdateSplitScreenLayout();

    // �ֱ������� ���� �÷��̾� ��ġ ����ȭ
    GetWorldTimerManager().SetTimer(
        SyncTimerHandle,
        this,
        &ASSGameMode::SyncDummyPlayerWithRemotePlayer,
        0.1f, // 10fps�� ����ȭ
        true
    );
}

void ASSGameMode::CreateDummyLocalPlayer()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // ���� ���� �÷��̾� �� Ȯ��
    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Already have 2+ local players"));
        return;
    }

    // ���� ���� �÷��̾� ���� // ���� (�𸮾� 5 ���)  
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, true);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player"));
        return;
    }

    // ���� ���������� �� ����
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

    // ���� �÷��̾� ��Ʈ�ѷ� ����
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

    // �� ��° �÷��̾� (���� �÷��̾�)�� ��ġ�� ���� �÷��̾ ����ȭ
    APlayerController* RemotePlayer = nullptr;

    // ù ��°�� �ƴ� �÷��̾� ã��
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
    // ����Ʈ ���̾ƿ��� ������ �ڵ����� ó��
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}
