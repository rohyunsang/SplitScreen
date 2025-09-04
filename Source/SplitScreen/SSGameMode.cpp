// Fill out your copyright notice in the Description page of Project Settings.


#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h" // FPlatformUserId ����� ���� �߰�
#include "TimerManager.h" // GetWorldTimerManager() ����� ����


ASSGameMode::ASSGameMode()
{
    // �⺻ Ŭ������ ����
    PlayerControllerClass = ASSPlayerController::StaticClass();

    // ���� ���������� �� Ŭ���� ����
    DummySpectatorPawnClass = ASSDummySpectatorPawn::StaticClass();

    // set default pawn class to our Blueprinted character
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (PlayerPawnBPClass.Class != NULL)
    {
        DefaultPawnClass = PlayerPawnBPClass.Class;
    }
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

    FString NetModeString = GetWorld()->GetNetMode() == NM_ListenServer ? TEXT("ListenServer") : TEXT("Client");
    UE_LOG(LogTemp, Warning, TEXT("SS Player Connected. Total Players: %d, NetMode: %s"),
        ConnectedPlayers.Num(), *NetModeString);

    if (bAutoEnableSplitScreen)
    {
        if (GetWorld()->GetNetMode() == NM_ListenServer)
        {
            // ����: ���� 2���� ���� ��
            if (ConnectedPlayers.Num() >= 2)
            {
                SetupOnlineSplitScreen();
            }
        }
        else if (GetWorld()->GetNetMode() == NM_Client)
        {
            // Ŭ���̾�Ʈ: �ڽ��� �����ϸ� �ٷ� ����
            // (�������� ������ �ٸ� �÷��̾ �� �غ�)
            SetupOnlineSplitScreen();
        }
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
        [this]()
        {
            SyncDummyPlayerWithRemotePlayer();
        },
        0.033f, // 30fps
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
        // return;
    }

    // ���� ���� �÷��̾� ����
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, true);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player"));
        return;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Success to create dummy local player"));
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

    // ���� �÷��̾� ã��
    APlayerController* RemotePlayer = nullptr;
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
    UCameraComponent* RemoteCamera = RemotePawn->FindComponentByClass<UCameraComponent>();

    if (RemoteCamera && DummySpectatorPawn->bSyncDirectlyToCamera)
    {
        // ��ǥ ��ġ�� ȸ��
        FVector TargetLocation = RemoteCamera->GetComponentLocation();
        FRotator TargetRotation = RemoteCamera->GetComponentRotation();

        // ���� ��ġ�� ȸ��
        FVector CurrentLocation = DummySpectatorPawn->GetActorLocation();
        FRotator CurrentRotation = DummySpectatorPawn->GetActorRotation();

        // ���� �ӵ� (���� Ŭ���� ������ ����)
        float InterpSpeed = 30.0f; // ���� ����
        float RotationInterpSpeed = 30.0f; // ȸ���� ���� �� ������

        // �Ÿ� üũ - �ʹ� �ָ� ��� �̵�
        float Distance = FVector::Dist(CurrentLocation, TargetLocation);
        if (Distance > 500.0f) // 5���� �̻� ���̳��� ��� �̵�
        {
            DummySpectatorPawn->SetActorLocation(TargetLocation);
            DummySpectatorPawn->SetActorRotation(TargetRotation);
        }
        else
        {
            // �ε巴�� ����
            FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, GetWorld()->GetDeltaSeconds(), InterpSpeed);
            FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);

            DummySpectatorPawn->SetActorLocation(NewLocation);
            DummySpectatorPawn->SetActorRotation(NewRotation);
        }

        // ��Ʈ�ѷ� ȸ���� ����
        if (DummyPlayerController)
        {
            FRotator ControlRotation = RemotePlayer->GetControlRotation();
            FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
            FRotator NewControlRotation = FMath::RInterpTo(CurrentControlRotation, ControlRotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);
            DummyPlayerController->SetControlRotation(NewControlRotation);
        }
    }
    
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // ����Ʈ ���̾ƿ��� ������ �ڵ����� ó��
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}