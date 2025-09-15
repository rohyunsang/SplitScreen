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

    // ���Ͻ� ���� �� ����
    SetupCameraProxies();

    // ������ ��쿡�� ����ȭ ����
    if (DummyPlayerController && DummySpectatorPawn && ClientCamProxy)
    {
        GetWorldTimerManager().SetTimer(
            SyncTimerHandle,
            [this]() { SyncDummyPlayerWithProxy(); },
            0.0083f,
            true
        );
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen setup completed successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Split screen setup failed"));
    }
}

void ASSGameMode::SetupCameraProxies()
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // ConnectedPlayers���� ���� �÷��̾� ã��
    APlayerController* HostPC = nullptr;
    APlayerController* ClientPC = nullptr;

    for (APlayerController* PC : ConnectedPlayers)
    {
        if (!PC || PC == DummyPlayerController) continue;

        if (PC->IsLocalController())
        {
            HostPC = PC; // ���������� ���� ��Ʈ�ѷ�
        }
        else
        {
            ClientPC = PC; // ���� Ŭ���̾�Ʈ
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Found Host PC: %s, Client PC: %s"),
        HostPC ? *HostPC->GetName() : TEXT("NULL"),
        ClientPC ? *ClientPC->GetName() : TEXT("NULL"));

    // 1) ���� ī�޶� ���Ͻ� (ȣ��Ʈ��)
    if (!ServerCamProxy)
    {
        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
        if (ServerCamProxy && HasAuthority() && HostPC)
        {
            ServerCamProxy->SetSourcePC(HostPC); // ���� PC ����
            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy with Host PC"));
        }
    }

    // 2) Ŭ���̾�Ʈ ī�޶� ���Ͻ� (Ŭ���̾�Ʈ��)
    if (!ClientCamProxy)
    {
        ClientCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
        if (ClientCamProxy && HasAuthority() && ClientPC)
        {
            ClientCamProxy->SetSourcePC(ClientPC); // ���� PC ����
            CachedClientProxy = ClientCamProxy;
            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy with Client PC"));
        }
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

void ASSGameMode::SyncDummyPlayerWithProxy()
{
    if (!DummySpectatorPawn) return;

    // ĳ�õ� ���Ͻ� ��� (���� ����ȭ)
    ASSCameraViewProxy* Proxy = CachedClientProxy.Get();
    if (!Proxy)
    {
        // ĳ�ð� ������ ClientCamProxy ���
        Proxy = ClientCamProxy;
        if (Proxy)
        {
            CachedClientProxy = Proxy;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS ClientCamProxy not found"));
            return;
        }
    }

    // Ŭ���̾�Ʈ ī�޶� �����͸� ���Ͻÿ��� ��������
    const FRepCamInfo& ClientCam = Proxy->GetReplicatedCamera();

    // ���Ͻ� ������ ����
    ApplyProxyCamera(DummySpectatorPawn, ClientCam);
}

void ASSGameMode::ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData)
{
    if (!DummyPawn) return;

    // �����: ���� ī�޶� ������ �α�
    UE_LOG(LogTemp, Log, TEXT("SS ApplyProxyCamera - Rot: %s, Loc: %s"),
        *CamData.Rotation.ToString(), *CamData.Location.ToString());

    // SpringArm ��� �� ��Ʈ�ѷ� ȸ���� �� �߿�
    if (DummyPlayerController)
    {
        // ���� ��Ʈ�ѷ� ȸ��
        FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();

        // �Ÿ� üũ - �ʹ� �ָ� ��� ����
        float Distance = FVector::Dist(DummyPawn->GetActorLocation(), CamData.Location);

        if (Distance > 500.0f)
        {
            // ��� ����
            DummyPlayerController->SetControlRotation(CamData.Rotation);
            DummyPawn->SetActorLocation(CamData.Location);
            UE_LOG(LogTemp, Warning, TEXT("SS Large distance detected: %.2f, immediate correction"), Distance);
        }
        else
        {
            // �ε巯�� ����
            float InterpSpeed = 35.0f;
            float RotationInterpSpeed = 45.0f;
            float DeltaTime = GetWorld()->GetDeltaSeconds();

            FVector CurrentLocation = DummyPawn->GetActorLocation();
            FVector NewLocation = FMath::VInterpTo(CurrentLocation, CamData.Location, DeltaTime, InterpSpeed);
            FRotator NewControlRotation = FMath::RInterpTo(CurrentControlRotation, CamData.Rotation, DeltaTime, RotationInterpSpeed);

            DummyPlayerController->SetControlRotation(NewControlRotation);
            DummyPawn->SetActorLocation(NewLocation);
        }
    }

    // FOV ����
    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        Camera->SetFieldOfView(CamData.FOV);
    }
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // ����Ʈ ���̾ƿ��� ������ �ڵ����� ó��
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}