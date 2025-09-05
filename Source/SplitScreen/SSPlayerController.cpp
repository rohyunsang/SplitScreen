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

        // ���� ���� ��Ʈ�ѷ��� �ִ��� üũ
        ASSPlayerController* ExistingDummyController = nullptr;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            ASSPlayerController* PC = Cast<ASSPlayerController>(*It);
            if (PC && PC->bIsDummyController && PC != this)
            {
                ExistingDummyController = PC;
                break;
            }
        }

        ASSPlayerController* DummyController = ExistingDummyController;

        // ���� ���� ��Ʈ�ѷ��� ���ٸ� ���� ����
        if (!DummyController)
        {
            DummyController = GetWorld()->SpawnActor<ASSPlayerController>();
            if (DummyController)
            {
                DummyController->SetAsDummyController(true);
                UE_LOG(LogTemp, Warning, TEXT("SS New dummy controller created: %s"), *DummyController->GetName());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Using existing dummy controller: %s"), *DummyController->GetName());
        }

        if (DummyController)
        {
            // �� ��° LocalPlayer ��������
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer)
                {
                    DummyController->SetPlayer(SecondLocalPlayer);
                    DummyController->Possess(ClientDummyPawn);
                    UE_LOG(LogTemp, Warning, TEXT("SS Client dummy controller setup complete"));
                }
            }
        }

        // Ŭ���̾�Ʈ ����ȭ ���� (�� ����)
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

    // 1) ���Ͻø� �� ã�Ҵٸ� �� ���� ��ĵ�ؼ� ĳ��
    ASSCameraViewProxy* Proxy = CachedProxy.Get();
    if (!Proxy)
    {
        for (TActorIterator<ASSCameraViewProxy> It(GetWorld()); It; ++It)
        {
            Proxy = *It;
            break; // ù ��°�� ���
        }
        if (!Proxy) return;
        CachedProxy = Proxy;
        UE_LOG(LogTemp, Warning, TEXT("SS Cached ServerCamProxy on client"));
    }

    // 2) ������ ���� ȭ�� ����(��ġ/ȸ��/FOV) ��������
    const FRepCamInfo& View = Proxy->GetReplicatedCamera();

    // ������ ���� �߰�
    if (View.Location.ContainsNaN() || View.Rotation.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Invalid camera data received, skipping update"));
        return;
    }

    // �ʹ� ū ��ȭ�� üũ
    const float MaxLocationChange = 2000.0f; // �����Ӵ� �ִ� �̵� �Ÿ�
    const float Dist = FVector::Dist(DummyPawn->GetActorLocation(), View.Location);

    if (Dist > MaxLocationChange)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Camera jump detected (%.2f units), smoothing transition"), Dist);
        // ū ��ȭ�� ���� �����ӿ� ���� ����
        const float Dt = GetWorld()->GetDeltaSeconds();
        const FVector NewLocation = FMath::VInterpTo(DummyPawn->GetActorLocation(), View.Location, Dt, 20.0f);
        const FRotator NewRotation = FMath::RInterpTo(DummyPawn->GetActorRotation(), View.Rotation, Dt, 30.0f);

        DummyPawn->SetActorLocation(NewLocation);
        DummyPawn->SetActorRotation(NewRotation);

        // ��Ʈ�ѷ� ȸ���� �Բ� ����
        if (APlayerController* DPC = Cast<APlayerController>(DummyPawn->GetController()))
        {
            const FRotator NewCtrlRot = FMath::RInterpTo(DPC->GetControlRotation(), View.Rotation, Dt, 30.0f);
            DPC->SetControlRotation(NewCtrlRot);
        }

        // FOV�� ����
        if (UCameraComponent* Cam = DummyPawn->FindComponentByClass<UCameraComponent>())
        {
            const float CurrentFOV = Cam->FieldOfView;
            const float NewFOV = FMath::FInterpTo(CurrentFOV, View.FOV, Dt, 10.0f);
            Cam->SetFieldOfView(NewFOV);
        }

        return; // ū ��ȭ ó�� �� ����
    }

    // 3) ���� ���� ���� - �Ϲ����� ������ ó��
    const float Dt = GetWorld()->GetDeltaSeconds();
    const float MoveSpd = 35.0f, RotSpd = 45.0f;

    // �Ÿ��� ���� ó�� �б�
    if (Dist > 500.f)
    {
        // �߰� �Ÿ� - ��� �̵������� ȸ���� ����
        DummyPawn->SetActorLocation(View.Location);
        DummyPawn->SetActorRotation(FMath::RInterpTo(DummyPawn->GetActorRotation(), View.Rotation, Dt, RotSpd));
    }
    else
    {
        // ����� �Ÿ� - �ε巯�� ����
        DummyPawn->SetActorLocation(FMath::VInterpTo(DummyPawn->GetActorLocation(), View.Location, Dt, MoveSpd));
        DummyPawn->SetActorRotation(FMath::RInterpTo(DummyPawn->GetActorRotation(), View.Rotation, Dt, RotSpd));
    }

    // 4) ���� ��Ʈ�ѷ��� ControlRotation�� ����
    if (APlayerController* DPC = Cast<APlayerController>(DummyPawn->GetController()))
    {
        const FRotator NewCtrlRot = FMath::RInterpTo(DPC->GetControlRotation(), View.Rotation, Dt, RotSpd);
        DPC->SetControlRotation(NewCtrlRot);
    }

    // 5) FOV ����ȭ (���� ī�޶� ��Ʈ�ѷ� ȸ���� ����ϵ��� ������ �־�� ��)
    if (UCameraComponent* Cam = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        const float CurrentFOV = Cam->FieldOfView;
        const float NewFOV = FMath::FInterpTo(CurrentFOV, View.FOV, Dt, 5.0f); // �ε巯�� FOV ��ȯ
        Cam->SetFieldOfView(NewFOV);
        // �ʿ� �� PostProcess �� �߰� ����ȭ ����
    }

    // 6) ������ �α� (ū ��ȭ ����)
    static FVector LastViewLocation = FVector::ZeroVector;
    const float LocationJump = FVector::Dist(View.Location, LastViewLocation);

    if (LocationJump > 1000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Large camera movement detected: %.2f units from %s to %s"),
            LocationJump, *LastViewLocation.ToString(), *View.Location.ToString());
    }

    LastViewLocation = View.Location;
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
void ASSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Ÿ�̸� ����
    if (GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(ClientSyncTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

*/