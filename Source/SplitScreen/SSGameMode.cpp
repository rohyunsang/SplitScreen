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
#include "Components/CapsuleComponent.h"

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

    // === �и��� Proxy �ý��� ===

    // 1) Ŭ���̾�Ʈ�� ���� Proxy ���� (��� ���� Ŭ���̾�Ʈ��)
    if (!NewPlayer->IsLocalController()) // ���� Ŭ���̾�Ʈ
    {
        FActorSpawnParameters ClientProxyParams;
        ClientProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ClientProxyParams.Owner = NewPlayer; // Ŭ���̾�Ʈ�� Owner�� ����

        ASSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ClientProxyParams
        );

        if (ClientProxy)
        {
            // �߿�: Ŭ���̾�Ʈ���� �����ǵ��� ����
            ClientProxy->SetReplicates(true);
            ClientProxy->SetReplicateMovement(false); // ī�޶� �����͸� ����

            // Ŭ���̾�Ʈ�� Proxy �ʿ� �߰�
            ClientCamProxies.Add(NewPlayer, ClientProxy);

            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy for %s (Owner: %s)"),
                *NewPlayer->GetName(), *ClientProxy->GetOwner()->GetName());
        }
    }

    // 2) ���� ���� �÷��̾�� Proxy ���� (�� ����)
    if (NewPlayer->IsLocalController() && !ServerCamProxy)
    {
        FActorSpawnParameters ServerProxyParams;
        ServerProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Owner�� �������� ���� - ���� ���� Proxy

        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ServerProxyParams
        );

        if (ServerCamProxy)
        {
            // ���� Proxy�� �����ǵ��� ����
            ServerCamProxy->SetReplicates(true);
            ServerCamProxy->SetReplicateMovement(false);

            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy (ListenServer POV, No Owner)"));
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
    FVector SpawnLocation = FVector(0, 0, 0);
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

    if (!DummySpectatorPawn || !DummyPlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS DummySpectatorPawn or DummyPlayerController invalid"));
        return;
    }

    // 3. ��ġ ����ȭ (ĳ���� ũ�⸸ŭ ������ ����)
    APawn* ClientPawn = RemoteClient->GetPawn();
    FVector TargetLoc = RemoteClientCam.Location; // �⺻��: Ŭ�� ī�޶� ��ġ �״��

    if (ClientPawn)
    {
        // 3-1) ���̷��� "head" ���� ����
        if (USkeletalMeshComponent* Mesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>())
        {
            if (Mesh->DoesSocketExist(TEXT("camera_socket")))
            {
                TargetLoc = Mesh->GetSocketLocation(TEXT("camera_socket"));
                // Socket�� ������ �⺻���� ĳ������ �߾��ε�. 
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS no head socket "));
        }
    }

    FVector NewLoc = FMath::VInterpTo(
        DummySpectatorPawn->GetActorLocation(),
        TargetLoc,
        GetWorld()->GetDeltaSeconds(),
        35.f // ���� �ӵ�
    );

    DummySpectatorPawn->SetActorLocation(NewLoc);

    // 4. ȸ���� Ŭ�� �Է°��� �״�� ���ų� ���� (�ɼ�)
    //    ���⼭�� Ŭ�� ī�޶� ȸ�� �״�� �ݿ�
    FRotator TargetRot = RemoteClientCam.Rotation;
    FRotator CurrentRot = DummyPlayerController->GetControlRotation();

    FRotator NewRot = FMath::RInterpTo(
        CurrentRot,
        TargetRot,
        GetWorld()->GetDeltaSeconds(),
        45.f // ���� �ӵ�
    );

    DummyPlayerController->SetControlRotation(NewRot);

    UE_LOG(LogTemp, Verbose, TEXT("SS Server: Synced dummy location=%s, rotation=%s"),
        *NewLoc.ToString(), *NewRot.ToString());
}

