// Fill out your copyright notice in the Description page of Project Settings.


#include "SSPlayerController.h"
#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "SSDummySpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "HAL/PlatformMisc.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "SSCameraViewProxy.h"

void ASSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!bIsDummyController)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Player Controller Started - IsLocalController: %s"),
            IsLocalController() ? TEXT("true") : TEXT("false"));

        // Ŭ���̾�Ʈ���� ���� ��Ʈ�ѷ��� ��� ���ø� ��ũ�� ����
        if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client detected - Setting up split screen"));

            // �̹� ������ �Ϸ�Ǿ����� üũ
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

            // ���� ���� �÷��̾� ���� (�� ����)
            FTimerHandle ClientSetupHandle;
            GetWorldTimerManager().SetTimer(
                ClientSetupHandle,
                [this]()
                {
                    if (!bClientSplitScreenSetupComplete) // �ٽ� �ѹ� üũ
                    {
                        SetupClientSplitScreen();
                    }
                },
                2.0f, // 2�� ����
                false
            );
        }
    }
}

void ASSPlayerController::SetupClientSplitScreen()
{
    // �̹� ���� �Ϸ�� ��� ����
    if (bClientSplitScreenSetupComplete)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client split screen setup already complete"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up client split screen"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();
    UE_LOG(LogTemp, Warning, TEXT("SS Client current local players: %d"), CurrentLocalPlayers);

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client already has 2+ local players"));

        // �̹� LocalPlayer�� �ִٸ� ���� ���� ���� (�� ����)
        if (!ClientDummyPawn)
        {
            CreateClientDummyPawn();
        }

        // ���� �Ϸ� �÷��� ����
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
        // LocalPlayer ���� ���� �� ���� �� ����
        CreateClientDummyPawn();

        // ���� �Ϸ� �÷��� ����
        bClientSplitScreenSetupComplete = true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Client failed to create dummy local player: %s"), *OutError);
    }
}

// SSPlayerController.cpp - CreateClientDummyPawn �Լ� ����
void ASSPlayerController::CreateClientDummyPawn()
{
    UE_LOG(LogTemp, Warning, TEXT("SS Creating client dummy pawn"));

    // �̹� ���� ���� �ִٸ� ����
    if (ClientDummyPawn && IsValid(ClientDummyPawn))
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn already exists and is valid"));
        return;
    }

    FVector DummySpawnLocation = FVector(0, 0, 200);
    FRotator DummySpawnRotation = FRotator::ZeroRotator;

    ClientDummyPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
        ASSDummySpectatorPawn::StaticClass(),
        DummySpawnLocation,
        DummySpawnRotation
    );

    if (ClientDummyPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn created successfully"));

        // *** �߿�: ���� ��Ʈ�ѷ��� ���� �������� �ʰ� ���� �� Ȱ�� ***

        // 1) ���� ���� ���� ��Ʈ�ѷ� ã��
        ASSPlayerController* ExistingDummyController = nullptr;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ASSPlayerController* PC = Cast<ASSPlayerController>(*It);
            if (PC && PC->bIsDummyController && PC != this)
            {
                ExistingDummyController = PC;
                UE_LOG(LogTemp, Warning, TEXT("SS Found existing dummy controller: %s"), *PC->GetName());
                break;
            }
        }

        ASSPlayerController* DummyController = ExistingDummyController;

        // 2) ���� ���� ��Ʈ�ѷ��� ���� ���� ���� ����
        if (!DummyController)
        {
            // �� ��Ʈ�ѷ� ���� ���� ���� �ʿ����� �ٽ� üũ
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer && !SecondLocalPlayer->PlayerController)
                {
                    // �� ��° LocalPlayer�� ��Ʈ�ѷ��� ���� ���� ����
                    DummyController = GetWorld()->SpawnActor<ASSPlayerController>();
                    if (DummyController)
                    {
                        DummyController->SetAsDummyController(true);
                        UE_LOG(LogTemp, Warning, TEXT("SS New dummy controller created: %s"), *DummyController->GetName());
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("SS SecondLocalPlayer already has controller, skipping creation"));
                    // �̹� ��Ʈ�ѷ��� �ִٸ� �װ��� ���
                    if (SecondLocalPlayer->PlayerController)
                    {
                        if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(SecondLocalPlayer->PlayerController))
                        {
                            DummyController = SSPC;
                            DummyController->SetAsDummyController(true);
                        }
                    }
                }
            }
        }

        // 3) ��Ʈ�ѷ� ����
        if (DummyController)
        {
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer)
                {
                    // ���� ������ ���� ���� ����
                    if (!SecondLocalPlayer->PlayerController || SecondLocalPlayer->PlayerController != DummyController)
                    {
                        DummyController->SetPlayer(SecondLocalPlayer);
                    }

                    // ���� �������� �ʾ��� ���� Possess
                    if (!ClientDummyPawn->GetController() || ClientDummyPawn->GetController() != DummyController)
                    {
                        DummyController->Possess(ClientDummyPawn);
                    }

                    UE_LOG(LogTemp, Warning, TEXT("SS Client dummy controller setup complete"));
                }
            }
        }

        // 4) Ŭ���̾�Ʈ ����ȭ ���� (�� ����)
        if (!GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
        {
            StartClientDummySync(ClientDummyPawn);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create client dummy pawn"));
    }
}


void ASSPlayerController::StartClientDummySync(ASSDummySpectatorPawn* DummyPawn)
{
    if (!DummyPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Cannot start sync - dummy pawn is null"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Starting client dummy sync"));

    // Ŭ���̾�Ʈ���� ���� �÷��̾�� ����ȭ
    GetWorldTimerManager().SetTimer(
        ClientSyncTimerHandle,
        [this, DummyPawn]()
        {
            SyncClientDummyWithRemotePlayer(DummyPawn);
        },
        0.0083f, 
        true
    );
}

void ASSPlayerController::SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn)
{
    if (!DummyPawn) return;

    // 1) ���Ͻÿ��� �ֽ� ���� ī�޶� ������ ��������
    ASSCameraViewProxy* Proxy = CachedProxy.Get();
    if (!Proxy)
    {
        for (TActorIterator<ASSCameraViewProxy> It(GetWorld()); It; ++It)
        {
            Proxy = *It;
            break;
        }
        if (!Proxy) return;
        CachedProxy = Proxy;
    }

    const FRepCamInfo& ServerCam = Proxy->GetReplicatedCamera();

    // 2) ���ο� ���� �����Ͱ� �����ߴ��� Ȯ��
    bool bNewServerData = false;
    if (!LastServerCamera.Location.Equals(ServerCam.Location, 1.0f) ||
        !LastServerCamera.Rotation.Equals(ServerCam.Rotation, 1.0f))
    {
        bNewServerData = true;
        UpdateCameraHistory(ServerCam);
    }

    // 3) ī�޶� ��ġ ���� ����
    FCameraPredictionData PredictedState = PredictCameraMovement();

    // 4) ���� �����ͷ� ���� ���� (�� �����Ͱ� ���� ����)
    if (bNewServerData)
    {
        PredictedState = CorrectPredictionWithServerData(PredictedState, ServerCam);
    }

    // 5) ���� ���� ������ ī�޶� ����
    ApplyPredictedCamera(DummyPawn, PredictedState);
}

void ASSPlayerController::UpdateCameraHistory(const FRepCamInfo& ServerCam)
{
    FCameraPredictionData NewData;
    NewData.Location = ServerCam.Location;
    NewData.Rotation = ServerCam.Rotation;
    NewData.FOV = ServerCam.FOV;
    NewData.Timestamp = GetWorld()->GetTimeSeconds();

    // �ӵ� ��� (���� �����Ͱ� �ִ� ���)
    if (CameraHistory.Num() > 0)
    {
        const FCameraPredictionData& LastData = CameraHistory.Last();
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
    CameraHistory.Add(NewData);

    // �����丮 ũ�� ����
    if (CameraHistory.Num() > MaxHistorySize)
    {
        CameraHistory.RemoveAt(0);
    }

    // ������ ���� ������ ������Ʈ
    LastServerCamera = NewData;
}

FCameraPredictionData ASSPlayerController::PredictCameraMovement()
{
    if (CameraHistory.Num() == 0)
    {
        return PredictedCamera; // �����Ͱ� ������ ���� ������ ����
    }

    const FCameraPredictionData& LatestData = CameraHistory.Last();
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float PredictionDelta = FMath::Clamp(CurrentTime - LatestData.Timestamp, 0.0f, MaxPredictionTime);

    FCameraPredictionData Predicted = LatestData;

    if (PredictionDelta > 0.0f && CameraHistory.Num() >= 2)
    {
        // ��ġ�� ����ó�� '�ӵ�(+���ӵ�)' ��� ����
        Predicted.Location = LatestData.Location + (LatestData.Velocity * PredictionDelta);

        if (CameraHistory.Num() >= 3)
        {
            const FCameraPredictionData& PrevData = CameraHistory[CameraHistory.Num() - 2];
            const float Den = FMath::Max(LatestData.Timestamp - PrevData.Timestamp, 0.001f);
            const FVector Accel = (LatestData.Velocity - PrevData.Velocity) / Den;
            Predicted.Location += 0.5f * Accel * PredictionDelta * PredictionDelta;
        }

        // [����] ȸ�� ���� ����: ���ӵ� ���� �� ��
        //       �� �׻� �ֽ� ���� �������� ȸ���� �״�� ���
        Predicted.Rotation = LatestData.Rotation;
    }
    else
    {
        // ��t�� 0�̰ų� �����丮�� �����ϸ� �״��
        Predicted.Location = LatestData.Location;
        Predicted.Rotation = LatestData.Rotation; // [����] ȸ�� ���� ����
    }

    Predicted.FOV = LatestData.FOV;
    PredictedCamera = Predicted;
    return Predicted;
}


FCameraPredictionData ASSPlayerController::CorrectPredictionWithServerData(
    const FCameraPredictionData& Prediction,
    const FRepCamInfo& ServerData)
{
    FCameraPredictionData Corrected = Prediction;

    // ���� �����Ϳ� ���� ������ ���� ���
    FVector LocationError = ServerData.Location - Prediction.Location;
    FRotator RotationError = (ServerData.Rotation - Prediction.Rotation).GetNormalized();

    // ������ �ʹ� ũ�� ��� ����, ������ ������ ����
    float LocationErrorMagnitude = LocationError.Size();
    float RotationErrorMagnitude = FMath::Abs(RotationError.Yaw) + FMath::Abs(RotationError.Pitch);

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    if (LocationErrorMagnitude > 100.0f) // 1���� �̻� ���̳��� ��� ����
    {
        Corrected.Location = ServerData.Location;
        UE_LOG(LogTemp, Warning, TEXT("Large location error detected: %.2f, immediate correction"), LocationErrorMagnitude);
    }
    else
    {
        // ������ ����
        Corrected.Location = FMath::VInterpTo(Prediction.Location, ServerData.Location, DeltaTime, CorrectionSpeed);
    }

    if (RotationErrorMagnitude > 10.0f) // 10�� �̻� ���̳��� ��� ����
    {
        Corrected.Rotation = ServerData.Rotation;
        UE_LOG(LogTemp, Warning, TEXT("Large rotation error detected: %.2f, immediate correction"), RotationErrorMagnitude);
    }
    else
    {
        // ������ ����
        Corrected.Rotation = FMath::RInterpTo(Prediction.Rotation, ServerData.Rotation, DeltaTime, CorrectionSpeed);
    }

    // FOV�� ��� ���� (�߿䵵 ����)
    Corrected.FOV = ServerData.FOV;

    return Corrected;
}

void ASSPlayerController::ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
    // ���� Ŭ�󿡼� ���� ĳ���� �������� ���̴�..
    for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
    {
        ACharacter* TargetCharacter = *It;
        if (!TargetCharacter || TargetCharacter->IsLocallyControlled())
            continue;

        // �ǹ�(���� ��)�� Ÿ�� ��ġ��
        const FVector Pivot = TargetCharacter->GetActorLocation(); // �ʿ�� �Ӹ� ���� ����
        DummyPawn->SetActorLocation(Pivot);

        // ��Ʈ�ѷ� ȸ���� ���������� �� ���������� �� ȸ���� �޾Ƽ� ���˵�
        if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
        {
            DummyController->SetControlRotation(CameraData.Rotation);
        }
        break;
    }

    // ī�޶� FOV ����
    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        Camera->SetFieldOfView(CameraData.FOV);
    }
}

// ����׿� �Լ� (���������� ���)
void ASSPlayerController::DebugCameraPrediction()
{
    if (CameraHistory.Num() > 0)
    {
        const FCameraPredictionData& Latest = CameraHistory.Last();
        UE_LOG(LogTemp, Log, TEXT("Camera Prediction - Velocity: %s, History Size: %d"),
            *Latest.Velocity.ToString(), CameraHistory.Num());
    }
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

    /*
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

    */
}


void ASSPlayerController::ServerUpdatePlayerLocation_Implementation(FVector Location, FRotator Rotation)
{
    // �������� �ٸ� Ŭ���̾�Ʈ�鿡�� ��ġ ���� ����
    ASSGameMode* SSGameMode = Cast<ASSGameMode>(GetWorld()->GetAuthGameMode());
    if (SSGameMode)
    {
        // ��� �ٸ� Ŭ���̾�Ʈ���� �� �÷��̾��� ��ġ ����
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ASSPlayerController* OtherPC = Cast<ASSPlayerController>(*It);
            if (OtherPC && OtherPC != this)
            {
                OtherPC->ClientReceiveRemotePlayerLocation(Location, Rotation);
            }
        }
    }
}

bool ASSPlayerController::ServerUpdatePlayerLocation_Validate(FVector Location, FRotator Rotation)
{
    return true;
}

void ASSPlayerController::ClientReceiveRemotePlayerLocation_Implementation(FVector Location, FRotator Rotation)
{
    // ���� ���� �÷��̾� ��ġ ���� �α�
    // UE_LOG(LogTemp, Log, TEXT("SS Received remote player location: %s"), *Location.ToString());
}

/*
// �߰�: PlayerController ���� �Լ�
void ASSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Ÿ�̸� ����
    if (GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(ClientSyncTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("SS Cleared sync timer for controller: %s"), *GetName());
    }

    // ���� ��Ʈ�ѷ��� ��� �߰� ����
    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy controller %s ending play"), *GetName());
    }

    Super::EndPlay(EndPlayReason);
}


*/