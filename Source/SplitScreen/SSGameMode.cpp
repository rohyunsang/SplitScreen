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
#include "SSCameraViewProxy.h"
#include "Kismet/GameplayStatics.h"


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
    UE_LOG(LogTemp, Warning, TEXT("SS PostLogin: %s, LocalController: %s, Total: %d, NetMode: %s"),
        *NewPlayer->GetName(),
        NewPlayer->IsLocalController() ? TEXT("Yes") : TEXT("No"),
        ConnectedPlayers.Num(),
        *NetModeString);

    if (bAutoEnableSplitScreen)
    {
        if (GetWorld()->GetNetMode() == NM_ListenServer)
        {
            // ��Ȯ�� 2���� ���� ���� (�ߺ� ����)
            if (ConnectedPlayers.Num() == 2 && !DummyPlayerController)
            {
                UE_LOG(LogTemp, Warning, TEXT("SS Starting split screen setup..."));
                SetupOnlineSplitScreen();
            }
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
    if (!SSGI || !SSGI->IsSplitScreenEnabled())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen not enabled in game instance"));
        return;
    }

    // �̹� ������ �����Ǿ� ������ ����
    if (DummyPlayerController && DummySpectatorPawn &&
        DummyPlayerController->GetLocalPlayer())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen already fully setup"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up online split screen..."));

    CreateDummyLocalPlayer();
    UpdateSplitScreenLayout();

    // ������ ��쿡�� ����ȭ ����
    if (DummyPlayerController && DummySpectatorPawn)
    {
        GetWorldTimerManager().SetTimer(
            SyncTimerHandle,
            [this]() { SyncDummyPlayerWithRemotePlayer(); },
            0.0083f,
            true
        );
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen setup completed successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Split screen setup failed"));
    }

    // 1) ���Ͻð� ������ ����
    if (!ServerCamProxy)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
        UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy"));
    }

    // 2) ���� ���� �÷��̾�(���� ����)�� ī�޶� �ҽ��� ����
    if (ServerCamProxy && HasAuthority())
    {
        // 0�� �ε��� PC = ���������� ȭ��
        ServerCamProxy->SetSourceFromPlayerIndex(0);
    }
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
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player"));
        return;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Success to create dummy local player"));
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
        // ���̷� ǥ��
        DummyPlayerController->SetAsDummyController(true);

        DummyPlayerController->SetPawn(nullptr);
        
        DummyPlayerController->SetPlayer(DummyLocalPlayer);
        DummyPlayerController->Possess(DummySpectatorPawn);

        UE_LOG(LogTemp, Warning, TEXT("SS Dummy Local Player Created Successfully"));
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
        float InterpSpeed = 35.0f; // ���� ����
        float RotationInterpSpeed = 45.0f; // ȸ���� ���� �� ������

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