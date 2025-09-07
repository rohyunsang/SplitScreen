// Fill out your copyright notice in the Description page of Project Settings.

#include "SSPlayerController.h"
#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SSDummySpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "HAL/PlatformMisc.h"
#include "EngineUtils.h"
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

        // ���� ��Ʈ�ѷ� ���� (���� �ڵ�� ����)
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

        if (!DummyController)
        {
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer && !SecondLocalPlayer->PlayerController)
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
                    UE_LOG(LogTemp, Warning, TEXT("SS SecondLocalPlayer already has controller, skipping creation"));
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

        if (DummyController)
        {
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer)
                {
                    if (!SecondLocalPlayer->PlayerController || SecondLocalPlayer->PlayerController != DummyController)
                    {
                        DummyController->SetPlayer(SecondLocalPlayer);
                    }

                    if (!ClientDummyPawn->GetController() || ClientDummyPawn->GetController() != DummyController)
                    {
                        DummyController->Possess(ClientDummyPawn);
                    }

                    UE_LOG(LogTemp, Warning, TEXT("SS Client dummy controller setup complete"));
                }
            }
        }

        // CharacterMovement ��� ����ȭ ����
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

    UE_LOG(LogTemp, Warning, TEXT("SS Starting client dummy sync with CharacterMovement"));

    // Ÿ�� ĳ���� ã��
    FindAndSetTargetCharacter();

    // ����ȭ Ÿ�̸� ���� (MovementSyncRate�� ����)
    float SyncInterval = 1.0f / MovementSyncRate;
    GetWorldTimerManager().SetTimer(
        ClientSyncTimerHandle,
        [this, DummyPawn]()
        {
            SyncClientDummyWithRemotePlayer(DummyPawn);
        },
        SyncInterval,
        true
    );
}

void ASSPlayerController::SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn)
{
    if (!DummyPawn) return;

    // Ÿ�� ĳ���Ͱ� ��ȿ���� ������ �ٽ� ã��
    if (!TargetCharacter.IsValid() || !TargetMovementComponent.IsValid())
    {
        FindAndSetTargetCharacter();
        if (!TargetCharacter.IsValid()) return;
    }

    // CharacterMovement ��� ����ȭ ����
    SyncCameraWithCharacterMovement(DummyPawn);
}

void ASSPlayerController::FindAndSetTargetCharacter()
{
    // ���� �÷��̾��� ĳ���� ã��
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC != this && !PC->IsLocalPlayerController())
        {
            if (ACharacter* RemoteCharacter = Cast<ACharacter>(PC->GetPawn()))
            {
                TargetCharacter = RemoteCharacter;
                TargetMovementComponent = RemoteCharacter->GetCharacterMovement();
                TargetCameraComponent = RemoteCharacter->FindComponentByClass<UCameraComponent>();

                UE_LOG(LogTemp, Warning, TEXT("SS Found target character: %s"),
                    *RemoteCharacter->GetName());

                if (TargetCameraComponent.IsValid())
                {
                    UE_LOG(LogTemp, Warning, TEXT("SS Found target camera in character"));
                }

                // �ʱ� ������ ����
                LastKnownLocation = RemoteCharacter->GetActorLocation();
                LastKnownRotation = RemoteCharacter->GetActorRotation();
                if (TargetMovementComponent.IsValid())
                {
                    LastKnownVelocity = TargetMovementComponent->Velocity;
                }
                LastSyncTime = GetWorld()->GetTimeSeconds();

                return;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SS No target character found"));
}

void ASSPlayerController::SyncCameraWithCharacterMovement(ASSDummySpectatorPawn* DummyPawn)
{
    if (!TargetCharacter.IsValid() || !TargetMovementComponent.IsValid())
    {
        return;
    }

    ACharacter* RemoteCharacter = TargetCharacter.Get();
    UCharacterMovementComponent* MovementComp = TargetMovementComponent.Get();

    // ���� �������� ���� ������
    FVector CurrentLocation = RemoteCharacter->GetActorLocation();
    FRotator CurrentRotation = RemoteCharacter->GetActorRotation();
    FVector CurrentVelocity = MovementComp->Velocity;
    float CurrentTime = GetWorld()->GetTimeSeconds();

    FVector TargetCameraLocation = CurrentLocation;
    FRotator TargetCameraRotation = CurrentRotation;

    // ī�޶� ������ ī�޶� ��ġ/ȸ�� ���
    if (TargetCameraComponent.IsValid())
    {
        TargetCameraLocation = TargetCameraComponent->GetComponentLocation();
        TargetCameraRotation = TargetCameraComponent->GetComponentRotation();
    }

    // CharacterMovement�� prediction Ȱ��
    if (bUsePredictiveSync && MovementComp->GetPredictionData_Client())
    {
        ApplyCharacterMovementPrediction(DummyPawn);
    }
    else
    {
        // �⺻ ����ȭ (���� ����)
        FVector CurrentDummyLocation = DummyPawn->GetActorLocation();
        FRotator CurrentDummyRotation = DummyPawn->GetActorRotation();

        // �Ÿ� üũ - �ʹ� �ָ� ��� �̵�
        float Distance = FVector::Dist(CurrentDummyLocation, TargetCameraLocation);
        if (Distance > MaxSyncDistance)
        {
            DummyPawn->SetActorLocation(TargetCameraLocation);
            DummyPawn->SetActorRotation(TargetCameraRotation);
            UE_LOG(LogTemp, Warning, TEXT("SS Large distance detected: %.2f, teleporting"), Distance);
        }
        else
        {
            // �ε巯�� ����
            float DeltaTime = GetWorld()->GetDeltaSeconds();
            FVector NewLocation = FMath::VInterpTo(CurrentDummyLocation, TargetCameraLocation, DeltaTime, CameraSmoothingSpeed);
            FRotator NewRotation = FMath::RInterpTo(CurrentDummyRotation, TargetCameraRotation, DeltaTime, CameraSmoothingSpeed);

            DummyPawn->SetActorLocation(NewLocation);
            DummyPawn->SetActorRotation(NewRotation);
        }
    }

    // ��Ʈ�ѷ� ȸ�� ����ȭ
    if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
    {
        if (TargetCameraComponent.IsValid())
        {
            DummyController->SetControlRotation(TargetCameraComponent->GetComponentRotation());
        }
        else
        {
            // ī�޶� ������ ĳ������ ��Ʈ�� ȸ�� ���
            if (APlayerController* TargetPC = Cast<APlayerController>(RemoteCharacter->GetController()))
            {
                DummyController->SetControlRotation(TargetPC->GetControlRotation());
            }
        }
    }

    // ���� ������ ������ ������Ʈ
    LastKnownLocation = CurrentLocation;
    LastKnownRotation = CurrentRotation;
    LastKnownVelocity = CurrentVelocity;
    LastSyncTime = CurrentTime;
}

void ASSPlayerController::ApplyCharacterMovementPrediction(ASSDummySpectatorPawn* DummyPawn)
{
    if (!TargetCharacter.IsValid() || !TargetMovementComponent.IsValid())
    {
        return;
    }

    ACharacter* RemoteCharacter = TargetCharacter.Get();
    UCharacterMovementComponent* MovementComp = TargetMovementComponent.Get();

    // CharacterMovement�� ���� ����
    FVector CurrentLocation = RemoteCharacter->GetActorLocation();
    FVector CurrentVelocity = MovementComp->Velocity;
    FRotator CurrentRotation = RemoteCharacter->GetActorRotation();

    // ���� �ð� ��� (��Ʈ��ũ ���� ����)
    float DeltaTime = GetWorld()->GetDeltaSeconds();
    float PredictionTime = DeltaTime * 2.0f; // �⺻������ 2������ ���� ����

    // CharacterMovement�� prediction ���� Ȱ��
    FVector PredictedLocation = PredictCharacterLocation(CurrentLocation, CurrentVelocity, PredictionTime);

    // ī�޶� ��ġ ���
    FVector TargetCameraLocation = PredictedLocation;
    FRotator TargetCameraRotation = CurrentRotation;

    if (TargetCameraComponent.IsValid())
    {
        // ī�޶� ������ ��� (ĳ���� ��ġ ��� ī�޶��� ��� ��ġ)
        FVector CameraOffset = TargetCameraComponent->GetComponentLocation() - CurrentLocation;
        TargetCameraLocation = PredictedLocation + CameraOffset;
        TargetCameraRotation = TargetCameraComponent->GetComponentRotation();
    }

    // ���� ���� ������ ī�޶� ����
    UpdateCameraFromCharacter(DummyPawn, TargetCameraLocation, TargetCameraRotation);
}

FVector ASSPlayerController::PredictCharacterLocation(const FVector& CurrentLocation, const FVector& Velocity, float DeltaTime)
{
    if (!TargetMovementComponent.IsValid())
    {
        return CurrentLocation;
    }

    UCharacterMovementComponent* MovementComp = TargetMovementComponent.Get();

    // �⺻ ���� ����
    FVector PredictedLocation = CurrentLocation + (Velocity * DeltaTime);

    // �߷� ���� (���߿� ���� ��)
    if (MovementComp->IsFalling())
    {
        float GravityZ = MovementComp->GetGravityZ();
        // �߷¿� ���� ���ӵ� ����: s = ut + 0.5*a*t^2
        PredictedLocation.Z += 0.5f * GravityZ * DeltaTime * DeltaTime;
    }

    // ���� �پ����� ���� Z�� ���� (������)
    if (MovementComp->IsMovingOnGround())
    {
        // �ʿ�� ���� ���� ���� �߰�
    }

    return PredictedLocation;
}

void ASSPlayerController::UpdateCameraFromCharacter(ASSDummySpectatorPawn* DummyPawn, const FVector& PredictedLocation, const FRotator& CharacterRotation)
{
    FVector CurrentDummyLocation = DummyPawn->GetActorLocation();
    FRotator CurrentDummyRotation = DummyPawn->GetActorRotation();

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    // �Ÿ� üũ
    float Distance = FVector::Dist(CurrentDummyLocation, PredictedLocation);
    if (Distance > MaxSyncDistance)
    {
        // ��� �̵�
        DummyPawn->SetActorLocation(PredictedLocation);
        DummyPawn->SetActorRotation(CharacterRotation);
    }
    else
    {
        // �ε巯�� ����
        FVector NewLocation = FMath::VInterpTo(CurrentDummyLocation, PredictedLocation, DeltaTime, CameraSmoothingSpeed);
        FRotator NewRotation = FMath::RInterpTo(CurrentDummyRotation, CharacterRotation, DeltaTime, CameraSmoothingSpeed);

        DummyPawn->SetActorLocation(NewLocation);
        DummyPawn->SetActorRotation(NewRotation);
    }
}

void ASSPlayerController::DebugMovementSync()
{
    if (TargetCharacter.IsValid() && TargetMovementComponent.IsValid())
    {
        UCharacterMovementComponent* MovementComp = TargetMovementComponent.Get();
        UE_LOG(LogTemp, Log, TEXT("Movement Sync - Velocity: %s, IsGrounded: %s, IsFalling: %s"),
            *MovementComp->Velocity.ToString(),
            MovementComp->IsMovingOnGround() ? TEXT("Yes") : TEXT("No"),
            MovementComp->IsFalling() ? TEXT("Yes") : TEXT("No"));
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
            ASSPlayerController* OtherPC = Cast<ASSPlayerController>(It->Get());
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