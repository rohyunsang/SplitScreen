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

    // Proxy ���� (����������)
    if (HasAuthority() && !NewPlayer->IsLocalController())
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = NewPlayer; // �ݵ�� Owner�� �ش� Ŭ���� PlayerController�� ����

        ASSCameraViewProxy* NewProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            Params
        );

        if (NewProxy)
        {
            ClientCamProxies.Add(NewPlayer, NewProxy);
            UE_LOG(LogTemp, Warning, TEXT("SS Proxy created for %s (Owner = %s)"),
                *NewPlayer->GetName(), *NewProxy->GetOwner()->GetName());
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
            // ��Ȯ�� 2���� ���� ����
            if (ConnectedPlayers.Num() == 2)
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

    CreateDummyLocalPlayer();
    // ���� Ŭ�� ã�� �� ���� ���������� ���̱�
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }
    AttachDummySpectatorToClient(RemoteClient);

    // === ȸ�� ����ȭ Ÿ�̸� ���� ===
    GetWorldTimerManager().SetTimer(
        RotationSyncTimerHandle,   // FTimerHandle ������� ���� �ʿ�
        this,
        &ASSGameMode::SyncDummyRotationWithProxy,
        0.016f,   // 60fps �ֱ� (16ms)
        true      // �ݺ�
    );

    UE_LOG(LogTemp, Warning, TEXT("SS SetupOnlineSplitScreen completed - Using client camera directly"));
}

void ASSGameMode::AttachDummySpectatorToClient(APlayerController* RemoteClient)
{
    if (!RemoteClient || !RemoteClient->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Remote client or pawn not valid"));
        return;
    }

    APawn* ClientPawn = RemoteClient->GetPawn();
    USkeletalMeshComponent* Mesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Client pawn has no skeletal mesh"));
        return;
    }

    if (!DummySpectatorPawn)
    {
        // ���� �� ����
        DummySpectatorPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
            DummySpectatorPawnClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator
        );
    }

    if (DummySpectatorPawn)
    {
        // Ŭ�� ĳ���� ���̷��� ���Ͽ� Attach
        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        DummySpectatorPawn->AttachToComponent(Mesh, AttachRules, FName("head"));
        // "head" ��� ĳ���� ���̷��� ���� �̸� ���

        // Pawn�� ������ �ʰ� ����
        DummySpectatorPawn->SetActorHiddenInGame(true);
        DummySpectatorPawn->SetActorEnableCollision(false);

        UE_LOG(LogTemp, Warning, TEXT("SS DummySpectator attached to %s's skeleton"),
            *ClientPawn->GetName());
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

void ASSGameMode::SyncDummyRotationWithProxy()
{
    UE_LOG(LogTemp, Warning, TEXT("SS SyncDummyRotationWithProxy called"));

    // 1. ���� Ŭ�� ã��
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }
    if (!RemoteClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS RemoteClient null"));
        return;
    }

    // 2. �ش� Ŭ���� Proxy ��������
    ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (!FoundProxy || !*FoundProxy)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS FoundProxy null"));
        return;
    }

    ASSCameraViewProxy* ClientProxy = *FoundProxy;
    const FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    // 3. ȸ���� ����ȭ
    FRotator TargetRot = RemoteClientCam.Rotation;
    FRotator CurrentRot = DummyPlayerController->GetControlRotation();

    // �ε巴�� ����
    FRotator NewRot = FMath::RInterpTo(
        CurrentRot,
        TargetRot,
        GetWorld()->GetDeltaSeconds(),
        45.f // ���� �ӵ�
    );

    DummyPlayerController->SetControlRotation(NewRot);

    UE_LOG(LogTemp, Verbose, TEXT("SS Server: Synced dummy rotation -> %s"), *NewRot.ToString());
}




///////////////////////////////////////////////////////////////////
// �� ������ ������. 


void ASSGameMode::SetClientCameraAsSecondView()
{
    // 1. ���� Ŭ���̾�Ʈ ã��
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (!RemoteClient || !RemoteClient->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No remote client pawn found"));
        return;
    }

    APawn* ClientPawn = RemoteClient->GetPawn();

    // 2. Pawn���� ī�޶� ã��
    UCameraComponent* ClientCamera = ClientPawn->FindComponentByClass<UCameraComponent>();
    if (!ClientCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Client pawn has no camera"));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // 3. �� ��° ���� �÷��̾� Ȯ��
    ULocalPlayer* SecondPlayer = GameInstance->GetLocalPlayerByIndex(1);
    if (!SecondPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No second local player"));
        return;
    }

    // 4. �� ��° ��Ʈ�ѷ� ���� (���� ���)
    if (!SecondPlayer->PlayerController)
    {
        APlayerController* SecondPC = GetWorld()->SpawnActor<APlayerController>();
        SecondPC->SetPlayer(SecondPlayer);
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Created controller for second player"));
    }

    // 5. �� ��° ����Ʈ �� Ŭ�� Pawn ī�޶�� ��ȯ
    if (APlayerController* SecondPC = SecondPlayer->PlayerController)
    {
        FViewTargetTransitionParams TransitionParams;
        SecondPC->SetViewTarget(ClientPawn, TransitionParams); // Pawn�� ���� CameraComponent ���

        UE_LOG(LogTemp, Warning, TEXT("SS Server: Second viewport now uses remote client camera"));
    }
}



void ASSGameMode::CreateSimpleDummyLocalPlayer()
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

    // ���� ���� �÷��̾� ���� (��/��Ʈ�ѷ� ����)
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Success to create dummy local player (no pawn)"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player"));
    }
}

void ASSGameMode::AttachCameraToRemoteClient()
{
    // ���� Ŭ���̾�Ʈ ã��
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (!RemoteClient || !RemoteClient->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No remote client or pawn found"));
        return;
    }

    APawn* ClientPawn = RemoteClient->GetPawn();
    USkeletalMeshComponent* ClientMesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>();

    if (!ClientMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No SkeletalMesh found in client pawn"));
        return;
    }

    // �̹� ī�޶� �ִ��� Ȯ��
    if (AttachedServerCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Camera already attached"));
        return;
    }

    // �� ī�޶� ������Ʈ ����
    AttachedServerCamera = NewObject<UCameraComponent>(ClientPawn, UCameraComponent::StaticClass(), TEXT("ServerViewCamera"));

    // �Ӹ� ���Ͽ� ���̱�
    FName HeadSocket = TEXT("head");
    if (ClientMesh->DoesSocketExist(HeadSocket))
    {
        AttachedServerCamera->SetupAttachment(ClientMesh, HeadSocket);
    }
    else
    {
        AttachedServerCamera->SetupAttachment(ClientMesh);
        AttachedServerCamera->SetRelativeLocation(FVector(0, 0, 160)); // �Ӹ� ����
    }

    // ī�޶� ���� - ȸ�� ����ȭ ���� ����!
    AttachedServerCamera->SetFieldOfView(90.0f);
    AttachedServerCamera->bUsePawnControlRotation = false; // �߿�: ��Ʈ�ѷ� ȸ�� ��� ����
    AttachedServerCamera->bAutoActivate = true;

    // ������Ʈ ���
    AttachedServerCamera->RegisterComponent();

    // �� ��° ����Ʈ ����
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        ULocalPlayer* SecondPlayer = GameInstance->GetLocalPlayerByIndex(1);
        if (SecondPlayer)
        {
            // �� ��° �÷��̾�� ��Ʈ�ѷ� ����/����
            if (!SecondPlayer->PlayerController)
            {
                APlayerController* SecondPC = GetWorld()->SpawnActor<APlayerController>();
                SecondPC->SetPlayer(SecondPlayer);
            }

            // Ŭ���̾�Ʈ ĳ���͸� ���� �� Ÿ������ ����
            if (APlayerController* SecondPC = SecondPlayer->PlayerController)
            {
                SecondPC->SetViewTarget(ClientPawn); // Ŭ���̾�Ʈ ĳ���͸� ���� Ÿ����

                UE_LOG(LogTemp, Warning, TEXT("SS Server: Second viewport set to client character with attached camera"));
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Server: Camera attached to client character without rotation sync"));
}

// �� ������ �Ⱦ� 


void ASSGameMode::SetSecondViewportCamera(UCameraComponent* Camera)
{
    if (!Camera) return;

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // �� ��° ���� �÷��̾� ��������
    ULocalPlayer* SecondPlayer = GameInstance->GetLocalPlayerByIndex(1);
    if (!SecondPlayer) return;

    // �� ��° �÷��̾�� ��Ʈ�ѷ� ���� (���� ��쿡��)
    if (!SecondPlayer->PlayerController)
    {
        APlayerController* SecondPC = GetWorld()->SpawnActor<APlayerController>();
        SecondPC->SetPlayer(SecondPlayer);

        UE_LOG(LogTemp, Warning, TEXT("SS Server: Created controller for second player"));
    }

    if (APlayerController* SecondPC = SecondPlayer->PlayerController)
    {
        // Ŭ���̾�Ʈ ĳ���͸� ���� �� Ÿ������ ����
        // �̷��� �ϸ� ���� ī�޶� �ڵ����� ����
        SecondPC->SetViewTarget(Camera->GetOwner());

        UE_LOG(LogTemp, Warning, TEXT("SS Server: Second viewport now views client character directly"));
    }
}


// �Ʒ��δ� ������ 


void ASSGameMode::SyncDummyPlayerWithRemotePlayer()
{
    if (!DummySpectatorPawn || !ServerCamProxy) return;

    // ���� Ŭ���̾�Ʈ ã�� (������ �ƴ� ù ��° �÷��̾�)
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (!RemoteClient)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SS Server: No remote client found"));
        return;
    }

    // �ش� Ŭ���̾�Ʈ�� Proxy ã��
    ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (!FoundProxy || !*FoundProxy)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No camera proxy found for remote client"));
        return;
    }

    ASSCameraViewProxy* ClientProxy = *FoundProxy;
    const FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    // === ���� �ý��� ���� ===

    // 1) ���ο� Ŭ���̾�Ʈ �����Ͱ� �����ߴ��� Ȯ��
    bool bNewClientData = false;
    if (!LastClientCamera.Location.Equals(RemoteClientCam.Location, 1.0f) ||
        !LastClientCamera.Rotation.Equals(RemoteClientCam.Rotation, 1.0f))
    {
        bNewClientData = true;
        UpdateClientCameraHistory(RemoteClientCam);
        UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: New client camera data - Rotation: %s"),
            *RemoteClientCam.Rotation.ToString());
    }

    // 2) ī�޶� ��ġ ���� ����
    FCameraPredictionData PredictedState = PredictClientCameraMovement();

    // 3) Ŭ���̾�Ʈ �����ͷ� ���� ���� (�� �����Ͱ� ���� ����)
    if (bNewClientData)
    {
        PredictedState = CorrectPredictionWithClientData(PredictedState, RemoteClientCam);
    }

    // 4) ���� ���� ������ ī�޶� ����
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

    // ���� Ŭ���̾�Ʈ�� �� ã��
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (RemoteClient && RemoteClient->GetPawn())
    {
        // Ŭ���̾�Ʈ �� ��ġ�� �ǹ����� ���
        FVector TargetPivot = RemoteClient->GetPawn()->GetActorLocation();

        // ���� ���� �� ��ġ
        FVector CurrentLocation = DummyPawn->GetActorLocation();
        float InterpSpeed = 35.f;

        // �Ÿ� ���̰� ũ�� �ٷ� ����
        float Distance = FVector::Dist(CurrentLocation, TargetPivot);
        if (Distance > 500.0f)
        {
            DummyPawn->SetActorLocation(TargetPivot);
            UE_LOG(LogTemp, Log, TEXT("SS Server: Dummy snapped to client at: %s"), *TargetPivot.ToString());
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
            DummyPawn->SetActorLocation(NewLocation);
        }
    }
    else
    {
        // Ŭ���̾�Ʈ ���� ������ ������ ī�޶� ��ġ ���� ���
        FVector CurrentLocation = DummyPawn->GetActorLocation();
        float Distance = FVector::Dist(CurrentLocation, CameraData.Location);

        if (Distance > 500.0f)
        {
            DummyPawn->SetActorLocation(CameraData.Location);
        }
        else
        {
            FVector NewLocation = FMath::VInterpTo(
                CurrentLocation,
                CameraData.Location,
                GetWorld()->GetDeltaSeconds(),
                35.f
            );
            DummyPawn->SetActorLocation(NewLocation);
        }
    }

    // ��Ʈ�ѷ� ȸ���� ������ Ŭ���̾�Ʈ ī�޶� ȸ������
    if (DummyPlayerController)
    {
        FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
        FRotator NewControlRotation = FMath::RInterpTo(
            CurrentControlRotation,
            CameraData.Rotation, // ������ ȸ�� ���
            GetWorld()->GetDeltaSeconds(),
            45.f
        );
        DummyPlayerController->SetControlRotation(NewControlRotation);

        UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Applied predicted client rotation: %s"),
            *NewControlRotation.ToString());
    }

    // ī�޶� FOV ����
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