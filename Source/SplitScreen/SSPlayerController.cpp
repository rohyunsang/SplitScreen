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

        // 클라이언트에서 로컬 컨트롤러인 경우 스플릿 스크린 설정
        if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client detected - Setting up split screen"));

            // 이미 설정이 완료되었는지 체크
            if (bClientSplitScreenSetupComplete)
            {
                UE_LOG(LogTemp, Warning, TEXT("SS Client split screen already setup, skipping"));
                return;
            }

            // GameInstance에서 스플릿 스크린 활성화
            if (USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance()))
            {
                SSGI->EnableSplitScreen();
            }

            // 더미 로컬 플레이어 생성 (한 번만)
            FTimerHandle ClientSetupHandle;
            GetWorldTimerManager().SetTimer(
                ClientSetupHandle,
                [this]()
                {
                    if (!bClientSplitScreenSetupComplete) // 다시 한번 체크
                    {
                        SetupClientSplitScreen();
                    }
                },
                2.0f, // 2초 지연
                false
            );
        }
    }
}

void ASSPlayerController::SetupClientSplitScreen()
{
    // 이미 설정 완료된 경우 리턴
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

        // 이미 LocalPlayer가 있다면 더미 폰만 생성 (한 번만)
        if (!ClientDummyPawn)
        {
            CreateClientDummyPawn();
        }

        // 설정 완료 플래그 설정
        bClientSplitScreenSetupComplete = true;
        return;
    }

    // 더미 로컬 플레이어 생성
    FPlatformUserId DummyUserId = FPlatformUserId::CreateFromInternalId(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy local player created successfully"));
        // LocalPlayer 생성 성공 후 더미 폰 생성
        CreateClientDummyPawn();

        // 설정 완료 플래그 설정
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

    // 이미 더미 폰이 있다면 리턴
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

        // 더미 컨트롤러 설정 (기존 코드와 동일)
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

        // CharacterMovement 기반 동기화 시작
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

    // 타겟 캐릭터 찾기
    FindAndSetTargetCharacter();

    // 동기화 타이머 설정 (MovementSyncRate에 맞춰)
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

    // 타겟 캐릭터가 유효하지 않으면 다시 찾기
    if (!TargetCharacter.IsValid() || !TargetMovementComponent.IsValid())
    {
        FindAndSetTargetCharacter();
        if (!TargetCharacter.IsValid()) return;
    }

    // CharacterMovement 기반 동기화 수행
    SyncCameraWithCharacterMovement(DummyPawn);
}

void ASSPlayerController::FindAndSetTargetCharacter()
{
    // 원격 플레이어의 캐릭터 찾기
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

                // 초기 데이터 설정
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

    // 현재 프레임의 실제 데이터
    FVector CurrentLocation = RemoteCharacter->GetActorLocation();
    FRotator CurrentRotation = RemoteCharacter->GetActorRotation();
    FVector CurrentVelocity = MovementComp->Velocity;
    float CurrentTime = GetWorld()->GetTimeSeconds();

    FVector TargetCameraLocation = CurrentLocation;
    FRotator TargetCameraRotation = CurrentRotation;

    // 카메라가 있으면 카메라 위치/회전 사용
    if (TargetCameraComponent.IsValid())
    {
        TargetCameraLocation = TargetCameraComponent->GetComponentLocation();
        TargetCameraRotation = TargetCameraComponent->GetComponentRotation();
    }

    // CharacterMovement의 prediction 활용
    if (bUsePredictiveSync && MovementComp->GetPredictionData_Client())
    {
        ApplyCharacterMovementPrediction(DummyPawn);
    }
    else
    {
        // 기본 동기화 (보간 적용)
        FVector CurrentDummyLocation = DummyPawn->GetActorLocation();
        FRotator CurrentDummyRotation = DummyPawn->GetActorRotation();

        // 거리 체크 - 너무 멀면 즉시 이동
        float Distance = FVector::Dist(CurrentDummyLocation, TargetCameraLocation);
        if (Distance > MaxSyncDistance)
        {
            DummyPawn->SetActorLocation(TargetCameraLocation);
            DummyPawn->SetActorRotation(TargetCameraRotation);
            UE_LOG(LogTemp, Warning, TEXT("SS Large distance detected: %.2f, teleporting"), Distance);
        }
        else
        {
            // 부드러운 보간
            float DeltaTime = GetWorld()->GetDeltaSeconds();
            FVector NewLocation = FMath::VInterpTo(CurrentDummyLocation, TargetCameraLocation, DeltaTime, CameraSmoothingSpeed);
            FRotator NewRotation = FMath::RInterpTo(CurrentDummyRotation, TargetCameraRotation, DeltaTime, CameraSmoothingSpeed);

            DummyPawn->SetActorLocation(NewLocation);
            DummyPawn->SetActorRotation(NewRotation);
        }
    }

    // 컨트롤러 회전 동기화
    if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
    {
        if (TargetCameraComponent.IsValid())
        {
            DummyController->SetControlRotation(TargetCameraComponent->GetComponentRotation());
        }
        else
        {
            // 카메라가 없으면 캐릭터의 컨트롤 회전 사용
            if (APlayerController* TargetPC = Cast<APlayerController>(RemoteCharacter->GetController()))
            {
                DummyController->SetControlRotation(TargetPC->GetControlRotation());
            }
        }
    }

    // 이전 프레임 데이터 업데이트
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

    // CharacterMovement의 현재 상태
    FVector CurrentLocation = RemoteCharacter->GetActorLocation();
    FVector CurrentVelocity = MovementComp->Velocity;
    FRotator CurrentRotation = RemoteCharacter->GetActorRotation();

    // 예측 시간 계산 (네트워크 지연 보상)
    float DeltaTime = GetWorld()->GetDeltaSeconds();
    float PredictionTime = DeltaTime * 2.0f; // 기본적으로 2프레임 앞을 예측

    // CharacterMovement의 prediction 정보 활용
    FVector PredictedLocation = PredictCharacterLocation(CurrentLocation, CurrentVelocity, PredictionTime);

    // 카메라 위치 계산
    FVector TargetCameraLocation = PredictedLocation;
    FRotator TargetCameraRotation = CurrentRotation;

    if (TargetCameraComponent.IsValid())
    {
        // 카메라 오프셋 계산 (캐릭터 위치 대비 카메라의 상대 위치)
        FVector CameraOffset = TargetCameraComponent->GetComponentLocation() - CurrentLocation;
        TargetCameraLocation = PredictedLocation + CameraOffset;
        TargetCameraRotation = TargetCameraComponent->GetComponentRotation();
    }

    // 더미 폰에 예측된 카메라 적용
    UpdateCameraFromCharacter(DummyPawn, TargetCameraLocation, TargetCameraRotation);
}

FVector ASSPlayerController::PredictCharacterLocation(const FVector& CurrentLocation, const FVector& Velocity, float DeltaTime)
{
    if (!TargetMovementComponent.IsValid())
    {
        return CurrentLocation;
    }

    UCharacterMovementComponent* MovementComp = TargetMovementComponent.Get();

    // 기본 선형 예측
    FVector PredictedLocation = CurrentLocation + (Velocity * DeltaTime);

    // 중력 적용 (공중에 있을 때)
    if (MovementComp->IsFalling())
    {
        float GravityZ = MovementComp->GetGravityZ();
        // 중력에 의한 가속도 적용: s = ut + 0.5*a*t^2
        PredictedLocation.Z += 0.5f * GravityZ * DeltaTime * DeltaTime;
    }

    // 땅에 붙어있을 때는 Z축 고정 (선택적)
    if (MovementComp->IsMovingOnGround())
    {
        // 필요시 지면 추적 로직 추가
    }

    return PredictedLocation;
}

void ASSPlayerController::UpdateCameraFromCharacter(ASSDummySpectatorPawn* DummyPawn, const FVector& PredictedLocation, const FRotator& CharacterRotation)
{
    FVector CurrentDummyLocation = DummyPawn->GetActorLocation();
    FRotator CurrentDummyRotation = DummyPawn->GetActorRotation();

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    // 거리 체크
    float Distance = FVector::Dist(CurrentDummyLocation, PredictedLocation);
    if (Distance > MaxSyncDistance)
    {
        // 즉시 이동
        DummyPawn->SetActorLocation(PredictedLocation);
        DummyPawn->SetActorRotation(CharacterRotation);
    }
    else
    {
        // 부드러운 보간
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

    // 더미 컨트롤러에서는 입력 처리 안함
    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy Controller - Skipping input setup"));
        return;
    }
}

void ASSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 더미 컨트롤러는 네트워크 동기화 안함
    if (bIsDummyController) return;

    // 로컬 플레이어만 위치 정보를 서버로 전송
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
    // 서버에서 다른 클라이언트들에게 위치 정보 전달
    ASSGameMode* SSGameMode = Cast<ASSGameMode>(GetWorld()->GetAuthGameMode());
    if (SSGameMode)
    {
        // 모든 다른 클라이언트에게 이 플레이어의 위치 전송
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
    // 받은 원격 플레이어 위치 정보 로그
    // UE_LOG(LogTemp, Log, TEXT("SS Received remote player location: %s"), *Location.ToString());
}