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

    if (!NewPlayer->IsLocalController()) // Ŭ���
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = NewPlayer; //  Ŭ�� ���� Proxy�� Owner�� Ŭ�� PC��

        ASSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ClientProxy)
        {
            ClientCamProxies.Add(NewPlayer, ClientProxy);
            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy for %s"), *NewPlayer->GetName());
        }
    }

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
    UE_LOG(LogTemp, Warning, TEXT("SSGameMode::SetupOnlineSplitScreen called"));

    // ���� ���� Proxy ����
    if (!ServerCamProxy)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ServerCamProxy && HasAuthority())
        {
            ServerCamProxy->SetSourceFromPlayerIndex(0); // ���� ���� ����
            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy (ListenServer POV)"));
        }
    }

    // ���� �÷��̾�/��Ʈ�ѷ� ����
    CreateDummyLocalPlayer();

    if (DummyPlayerController && DummySpectatorPawn)
    {
        GetWorldTimerManager().SetTimer(
            SyncTimerHandle,
            [this]() { SyncDummyPlayerWithRemotePlayer(); },
            0.0083f,
            true
        );

        UE_LOG(LogTemp, Warning, TEXT("SS SetupOnlineSplitScreen completed"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to complete SetupOnlineSplitScreen"));
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
    if (!DummySpectatorPawn || !ServerCamProxy) return;

    // *** �ٽ� ����: ��Ȯ�ϰ� ���� Ŭ���̾�Ʈ�� Ÿ������ ���� ***

    // ���� Ŭ���̾�Ʈ ã�� (������ �ƴ� �÷��̾�)
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Found remote client: %s"), *PC->GetName());
            break;
        }
    }

    if (!RemoteClient)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SS No remote client found for server dummy sync"));
        return;
    }

    // �ش� Ŭ���̾�Ʈ�� Proxy ã��
    ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (!FoundProxy || !*FoundProxy)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS No camera proxy found for remote client"));
        return;
    }

    ASSCameraViewProxy* ClientProxy = *FoundProxy;
    const FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    // ���� Ŭ���̾�Ʈ�� �� ��ġ�� �������� ���� �� ����ȭ
    if (RemoteClient->GetPawn())
    {
        FVector TargetPivot = RemoteClient->GetPawn()->GetActorLocation();

        // ���� DummyPawn ��ġ/ȸ�� ��������
        FVector CurrentLocation = DummySpectatorPawn->GetActorLocation();
        FRotator CurrentRotation = DummySpectatorPawn->GetActorRotation();

        float InterpSpeed = 35.f;
        float RotationInterpSpeed = 45.f;

        // �Ÿ� ���̰� ũ�� �ٷ� ����
        float Distance = FVector::Dist(CurrentLocation, TargetPivot);
        if (Distance > 500.0f)
        {
            DummySpectatorPawn->SetActorLocation(TargetPivot);
            UE_LOG(LogTemp, Log, TEXT("SS Server dummy snapped to remote client at: %s"), *TargetPivot.ToString());
        }
        else
        {
            // �ε巴�� ���� - Ŭ���̾�Ʈ �� ��ġ��
            FVector NewLocation = FMath::VInterpTo(
                CurrentLocation,
                TargetPivot,
                GetWorld()->GetDeltaSeconds(),
                InterpSpeed
            );
            DummySpectatorPawn->SetActorLocation(NewLocation);
        }

        // ��Ʈ�ѷ� ȸ���� Ŭ���̾�Ʈ ī�޶� ȸ������
        if (DummyPlayerController)
        {
            FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
            FRotator NewControlRotation = FMath::RInterpTo(
                CurrentControlRotation,
                RemoteClientCam.Rotation,
                GetWorld()->GetDeltaSeconds(),
                RotationInterpSpeed
            );
            DummyPlayerController->SetControlRotation(NewControlRotation);
        }
    }
    else
    {
        // Ŭ���̾�Ʈ ���� ������ ī�޶� ��ġ ���� ���
        FVector CurrentLocation = DummySpectatorPawn->GetActorLocation();
        float Distance = FVector::Dist(CurrentLocation, RemoteClientCam.Location);

        if (Distance > 500.0f)
        {
            DummySpectatorPawn->SetActorLocation(RemoteClientCam.Location);
        }
        else
        {
            FVector NewLocation = FMath::VInterpTo(
                CurrentLocation,
                RemoteClientCam.Location,
                GetWorld()->GetDeltaSeconds(),
                35.f
            );
            DummySpectatorPawn->SetActorLocation(NewLocation);
        }

        if (DummyPlayerController)
        {
            FRotator NewControlRotation = FMath::RInterpTo(
                DummyPlayerController->GetControlRotation(),
                RemoteClientCam.Rotation,
                GetWorld()->GetDeltaSeconds(),
                45.f
            );
            DummyPlayerController->SetControlRotation(NewControlRotation);
        }
    }
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // ����Ʈ ���̾ƿ��� ������ �ڵ����� ó��
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}