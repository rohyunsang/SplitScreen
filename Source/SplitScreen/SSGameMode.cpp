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
#include "Camera/CameraComponent.h"

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

    // *** Ŭ���̾�Ʈ�� �������� ���� Ŭ���̾�Ʈ ���� Proxy ���� ***
    if (!NewPlayer->IsLocalController()) // ���� Ŭ���̾�Ʈ
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = NewPlayer; // Ŭ���̾�Ʈ ���� Proxy�� Owner�� Ŭ���̾�Ʈ PC�� ����

        ASSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ClientProxy)
        {
            ClientProxy->SetOwner(NewPlayer);   // ��������� ������ ����
            ClientProxy->SetReplicates(true);   // ���� Ȱ��ȭ
            ClientCamProxies.Add(NewPlayer, ClientProxy);

            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy for %s (Owner: %s)"),
                *NewPlayer->GetName(), *ClientProxy->GetOwner()->GetName());
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

    // *** ���� ���� Proxy ���� (Owner ����) ***
    if (!ServerCamProxy)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Owner�� �������� ���� - ���� ���� Proxy

        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ServerCamProxy && HasAuthority())
        {
            ServerCamProxy->SetSourceFromPlayerIndex(0); // ���� ���� ���� (PlayerIndex 0)
            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy (ListenServer POV, No Owner)"));
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
    if (!DummySpectatorPawn) return;

    // === ���� Ŭ�� ã�� ===
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController()) // ���� Ŭ��
        {
            RemoteClient = PC;
            break;
        }
    }
    if (!RemoteClient) return;

    // === �ش� Ŭ���� Proxy �������� ===
    ASSCameraViewProxy* ClientProxy = nullptr;
    if (ClientCamProxies.Contains(RemoteClient))
    {
        ClientProxy = ClientCamProxies[RemoteClient];
    }
    if (!ClientProxy) return;

    // === �ֽ� Ŭ�� View �������� ===
    const FRepPlayerView& ClientView = ClientProxy->GetReplicatedView();

    bool bNewData = false;
    if (!LastClientCamera.Location.Equals(ClientView.CharacterLocation, 1.0f) ||
        !LastClientCamera.Rotation.Equals(ClientView.CameraRotation, 1.0f))
    {
        bNewData = true;
        UpdateClientCameraHistory(FRepCamInfo{
            ClientView.CharacterLocation,
            ClientView.CameraRotation,
            ClientView.FOV
            });
    }

    FCameraPredictionData PredictedState = PredictClientCameraMovement();
    if (bNewData)
    {
        PredictedState = CorrectPredictionWithClientData(
            PredictedState,
            FRepCamInfo{ ClientView.CharacterLocation, ClientView.CameraRotation, ClientView.FOV }
        );
    }

    ApplyPredictedClientCamera(DummySpectatorPawn, PredictedState);
}



// === ī�޶� ���� �ý��� ���� �Լ��� ===

void ASSGameMode::UpdateClientCameraHistory(const FRepCamInfo& ClientCam)
{
    FCameraPredictionData NewData;
    NewData.Location = ClientCam.Location;
    NewData.Rotation = ClientCam.Rotation;
    NewData.FOV = ClientCam.FOV;
    NewData.Timestamp = GetWorld()->GetTimeSeconds();

    // �ӵ� ��� (���� �����Ͱ� �ִ� ���)
    if (ClientCameraHistory.Num() > 0)
    {
        const FCameraPredictionData& LastData = ClientCameraHistory.Last();
        float DeltaTime = NewData.Timestamp - LastData.Timestamp;

        if (DeltaTime > 0.0f)
        {
            // ���� �ӵ� ���
            NewData.Velocity = (NewData.Location - LastData.Location) / DeltaTime;

            // ���ӵ� ��� (�ܼ�ȭ�� ����)
            FRotator DeltaRotation = (NewData.Rotation - LastData.Rotation).GetNormalized();
            NewData.AngularVelocity = FVector(DeltaRotation.Pitch, DeltaRotation.Yaw, DeltaRotation.Roll) / DeltaTime;
        }
    }

    // �����丮�� �߰�
    ClientCameraHistory.Add(NewData);

    // �����丮 ũ�� ����
    if (ClientCameraHistory.Num() > MaxHistorySize)
    {
        ClientCameraHistory.RemoveAt(0);
    }

    // ������ Ŭ���̾�Ʈ ������ ������Ʈ
    LastClientCamera = NewData;
}

FCameraPredictionData ASSGameMode::PredictClientCameraMovement()
{
    if (ClientCameraHistory.Num() == 0)
    {
        return PredictedClientCamera; // �����Ͱ� ������ ���� ������ ����
    }

    const FCameraPredictionData& LatestData = ClientCameraHistory.Last();
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float PredictionDelta = FMath::Clamp(CurrentTime - LatestData.Timestamp, 0.0f, MaxPredictionTime);

    FCameraPredictionData Predicted = LatestData;

    if (PredictionDelta > 0.0f && ClientCameraHistory.Num() >= 2)
    {
        // ��ġ�� �ӵ�(+���ӵ�) ��� ����
        Predicted.Location = LatestData.Location + (LatestData.Velocity * PredictionDelta);

        if (ClientCameraHistory.Num() >= 3)
        {
            const FCameraPredictionData& PrevData = ClientCameraHistory[ClientCameraHistory.Num() - 2];
            const float Den = FMath::Max(LatestData.Timestamp - PrevData.Timestamp, 0.001f);
            const FVector Accel = (LatestData.Velocity - PrevData.Velocity) / Den;
            Predicted.Location += 0.5f * Accel * PredictionDelta * PredictionDelta;
        }

        // ȸ�� ���� ����: �׻� �ֽ� Ŭ���̾�Ʈ �������� ȸ���� �״�� ���
        Predicted.Rotation = LatestData.Rotation;
    }
    else
    {
        // ��t�� 0�̰ų� �����丮�� �����ϸ� �״��
        Predicted.Location = LatestData.Location;
        Predicted.Rotation = LatestData.Rotation;
    }

    Predicted.FOV = LatestData.FOV;
    PredictedClientCamera = Predicted;
    return Predicted;
}

FCameraPredictionData ASSGameMode::CorrectPredictionWithClientData(
    const FCameraPredictionData& Prediction,
    const FRepCamInfo& ClientData)
{
    FCameraPredictionData Corrected = Prediction;

    // Ŭ���̾�Ʈ �����Ϳ� ���� ������ ���� ���
    FVector LocationError = ClientData.Location - Prediction.Location;
    FRotator RotationError = (ClientData.Rotation - Prediction.Rotation).GetNormalized();

    // ������ �ʹ� ũ�� ��� ����, ������ ������ ����
    float LocationErrorMagnitude = LocationError.Size();
    float RotationErrorMagnitude = FMath::Abs(RotationError.Yaw) + FMath::Abs(RotationError.Pitch);

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    if (LocationErrorMagnitude > ImmediateCorrectionLocationThreshold) // 1���� �̻� ���̳��� ��� ����
    {
        Corrected.Location = ClientData.Location;
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Large location error detected: %.2f, immediate correction"), LocationErrorMagnitude);
    }
    else
    {
        // ������ ����
        Corrected.Location = FMath::VInterpTo(Prediction.Location, ClientData.Location, DeltaTime, CorrectionSpeed);
    }

    if (RotationErrorMagnitude > ImmediateCorrectionRotationThreshold) // 10�� �̻� ���̳��� ��� ����
    {
        Corrected.Rotation = ClientData.Rotation;
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Large rotation error detected: %.2f, immediate correction"), RotationErrorMagnitude);
    }
    else
    {
        // ������ ����
        Corrected.Rotation = FMath::RInterpTo(Prediction.Rotation, ClientData.Rotation, DeltaTime, CorrectionSpeed);
    }

    // FOV�� ��� ���� (�߿䵵 ����)
    Corrected.FOV = ClientData.FOV;

    return Corrected;
}

void ASSGameMode::ApplyPredictedClientCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
    if (!DummyPawn) return;

    // === ��ġ ���� (����/���� �� ���) ===
    FVector CurrentLocation = DummyPawn->GetActorLocation();
    float Distance = FVector::Dist(CurrentLocation, CameraData.Location);

    if (Distance > 200.0f) // ū ���� �� ��� ����
    {
        DummyPawn->SetActorLocation(CameraData.Location);
        UE_LOG(LogTemp, Log, TEXT("SS Server: Dummy snapped to predicted location (%.1fcm error)"), Distance);
    }
    else
    {
        FVector NewLocation = FMath::VInterpTo(
            CurrentLocation,
            CameraData.Location,
            GetWorld()->GetDeltaSeconds(),
            15.f // ���� �ӵ�
        );
        DummyPawn->SetActorLocation(NewLocation);
    }

    // === ȸ�� ���� ===
    if (DummyPlayerController)
    {
        FRotator CurrentRot = DummyPlayerController->GetControlRotation();
        FRotator NewRot = FMath::RInterpTo(
            CurrentRot,
            CameraData.Rotation,
            GetWorld()->GetDeltaSeconds(),
            20.f
        );
        DummyPlayerController->SetControlRotation(NewRot);
    }

    // === FOV ���� ===
    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        Camera->SetFieldOfView(CameraData.FOV);
    }
}



void ASSGameMode::UpdateSplitScreenLayout()
{
    // ����Ʈ ���̾ƿ��� ������ �ڵ����� ó��
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}

// ����׿� �Լ�
void ASSGameMode::DebugServerCameraPrediction()
{
    if (ClientCameraHistory.Num() > 0)
    {
        const FCameraPredictionData& Latest = ClientCameraHistory.Last();
        UE_LOG(LogTemp, Log, TEXT("SS Server Camera Prediction - Velocity: %s, History Size: %d"),
            *Latest.Velocity.ToString(), ClientCameraHistory.Num());
    }
}