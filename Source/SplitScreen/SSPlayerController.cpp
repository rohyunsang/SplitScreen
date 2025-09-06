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

// SSPlayerController.cpp - CreateClientDummyPawn 함수 수정
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

        // *** 중요: 더미 컨트롤러를 새로 생성하지 않고 기존 것 활용 ***

        // 1) 먼저 기존 더미 컨트롤러 찾기
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

        // 2) 기존 더미 컨트롤러가 없을 때만 새로 생성
        if (!DummyController)
        {
            // 새 컨트롤러 생성 전에 정말 필요한지 다시 체크
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer && !SecondLocalPlayer->PlayerController)
                {
                    // 두 번째 LocalPlayer에 컨트롤러가 없을 때만 생성
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
                    // 이미 컨트롤러가 있다면 그것을 사용
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

        // 3) 컨트롤러 설정
        if (DummyController)
        {
            UGameInstance* GameInstance = GetGameInstance();
            if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
            {
                ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
                if (SecondLocalPlayer)
                {
                    // 기존 연결이 없을 때만 설정
                    if (!SecondLocalPlayer->PlayerController || SecondLocalPlayer->PlayerController != DummyController)
                    {
                        DummyController->SetPlayer(SecondLocalPlayer);
                    }

                    // 폰이 소유되지 않았을 때만 Possess
                    if (!ClientDummyPawn->GetController() || ClientDummyPawn->GetController() != DummyController)
                    {
                        DummyController->Possess(ClientDummyPawn);
                    }

                    UE_LOG(LogTemp, Warning, TEXT("SS Client dummy controller setup complete"));
                }
            }
        }

        // 4) 클라이언트 동기화 시작 (한 번만)
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

    // 클라이언트에서 원격 플레이어와 동기화
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

    // 1) 프록시에서 최신 서버 카메라 데이터 가져오기
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

    // 2) 새로운 서버 데이터가 도착했는지 확인
    bool bNewServerData = false;
    if (!LastServerCamera.Location.Equals(ServerCam.Location, 1.0f) ||
        !LastServerCamera.Rotation.Equals(ServerCam.Rotation, 1.0f))
    {
        bNewServerData = true;
        UpdateCameraHistory(ServerCam);
    }

    // 3) 카메라 위치 예측 수행
    FCameraPredictionData PredictedState = PredictCameraMovement();

    // 4) 서버 데이터로 예측 보정 (새 데이터가 있을 때만)
    if (bNewServerData)
    {
        PredictedState = CorrectPredictionWithServerData(PredictedState, ServerCam);
    }

    // 5) 더미 폰에 예측된 카메라 적용
    ApplyPredictedCamera(DummyPawn, PredictedState);
}

void ASSPlayerController::UpdateCameraHistory(const FRepCamInfo& ServerCam)
{
    FCameraPredictionData NewData;
    NewData.Location = ServerCam.Location;
    NewData.Rotation = ServerCam.Rotation;
    NewData.FOV = ServerCam.FOV;
    NewData.Timestamp = GetWorld()->GetTimeSeconds();

    // 속도 계산 (이전 데이터가 있는 경우)
    if (CameraHistory.Num() > 0)
    {
        const FCameraPredictionData& LastData = CameraHistory.Last();
        float DeltaTime = NewData.Timestamp - LastData.Timestamp;

        if (DeltaTime > 0.0f)
        {
            // 위치 변화량 계산
            FVector LocationDelta = NewData.Location - LastData.Location;
            float LocationDeltaMagnitude = LocationDelta.Size();

            // 회전 변화량 계산
            FRotator RotationDelta = (NewData.Rotation - LastData.Rotation).GetNormalized();
            float RotationDeltaMagnitude = FMath::Sqrt(
                RotationDelta.Pitch * RotationDelta.Pitch +
                RotationDelta.Yaw * RotationDelta.Yaw +
                RotationDelta.Roll * RotationDelta.Roll
            );

            // 움직임 상태 판단
            bIsMoving = LocationDeltaMagnitude > MinimumMovementThreshold;
            bIsRotating = RotationDeltaMagnitude > MinimumRotationThreshold;

            if (!bIsMoving && !bIsRotating)
            {
                StationaryTime += DeltaTime;
            }
            else
            {
                StationaryTime = 0.0f;
            }

            // 속도 계산 (움직이고 있을 때만)
            if (bIsMoving)
            {
                NewData.Velocity = LocationDelta / DeltaTime;
            }
            else
            {
                // 정지 상태에서는 속도를 점진적으로 감소
                NewData.Velocity = LastData.Velocity * 0.8f; // 감쇠 계수
                if (NewData.Velocity.Size() < 1.0f) // 1cm/s 이하면 0으로
                {
                    NewData.Velocity = FVector::ZeroVector;
                }
            }

            // 각속도 계산 (회전하고 있을 때만)
            if (bIsRotating)
            {
                NewData.AngularVelocity = FVector(
                    RotationDelta.Pitch,
                    RotationDelta.Yaw,
                    RotationDelta.Roll
                ) / DeltaTime;

                // 각속도 제한 적용
                float AngularMagnitude = NewData.AngularVelocity.Size();
                if (AngularMagnitude > MaxAngularVelocityMagnitude)
                {
                    NewData.AngularVelocity = NewData.AngularVelocity.GetSafeNormal() * MaxAngularVelocityMagnitude;
                }
            }
            else
            {
                // 정지 상태에서는 각속도를 점진적으로 감소
                NewData.AngularVelocity = LastData.AngularVelocity * 0.7f; // 더 빠른 감쇠
                if (NewData.AngularVelocity.Size() < 0.5f) // 0.5도/s 이하면 0으로
                {
                    NewData.AngularVelocity = FVector::ZeroVector;
                }
            }
        }
    }

    // 히스토리에 추가
    CameraHistory.Add(NewData);

    // 히스토리 크기 제한
    if (CameraHistory.Num() > MaxHistorySize)
    {
        CameraHistory.RemoveAt(0);
    }

    // 마지막 서버 데이터 업데이트
    LastServerCamera = NewData;
}
FCameraPredictionData ASSPlayerController::PredictCameraMovement()
{
    if (CameraHistory.Num() == 0)
    {
        return PredictedCamera;
    }

    const FCameraPredictionData& LatestData = CameraHistory.Last();
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float PredictionDelta = FMath::Clamp(CurrentTime - LatestData.Timestamp, 0.0f, MaxPredictionTime);

    FCameraPredictionData Predicted = LatestData;

    if (PredictionDelta > 0.0f && CameraHistory.Num() >= 2)
    {
        // 정지 상태에 따른 예측 강도 조절
        float PredictionStrength = 1.0f;
        if (StationaryTime > 0.5f) // 0.5초 이상 정지
        {
            PredictionStrength = StationaryPredictionReduction;
        }
        else if (StationaryTime > 0.1f) // 0.1초 이상 정지 시 점진적 감소
        {
            float t = (StationaryTime - 0.1f) / 0.4f; // 0.1~0.5초 구간을 0~1로 정규화
            PredictionStrength = FMath::Lerp(1.0f, StationaryPredictionReduction, t);
        }

        // 위치 예측 (움직이고 있을 때만 적극적으로)
        if (bIsMoving && LatestData.Velocity.Size() > 1.0f)
        {
            FVector PredictedVelocity = LatestData.Velocity * PredictionStrength;
            Predicted.Location = LatestData.Location + (PredictedVelocity * PredictionDelta);

            // 가속도 기반 보정 (움직임이 있을 때만)
            if (CameraHistory.Num() >= 3)
            {
                const FCameraPredictionData& PrevData = CameraHistory[CameraHistory.Num() - 2];
                FVector Acceleration = (LatestData.Velocity - PrevData.Velocity) /
                    FMath::Max(LatestData.Timestamp - PrevData.Timestamp, 0.001f);

                // 가속도 제한
                float AccelerationMagnitude = Acceleration.Size();
                if (AccelerationMagnitude > 1000.0f) // 10m/s² 제한
                {
                    Acceleration = Acceleration.GetSafeNormal() * 1000.0f;
                }

                Predicted.Location += 0.5f * Acceleration * PredictionStrength * PredictionDelta * PredictionDelta;
            }
        }

        // 회전 예측 (회전하고 있을 때만, 더 보수적으로)
        if (bIsRotating && LatestData.AngularVelocity.Size() > 0.5f)
        {
            FVector PredictedAngularVel = LatestData.AngularVelocity * PredictionStrength * 0.5f; // 회전은 더 보수적

            FRotator PredictedRotation = LatestData.Rotation;
            PredictedRotation.Pitch += PredictedAngularVel.X * PredictionDelta;
            PredictedRotation.Yaw += PredictedAngularVel.Y * PredictionDelta;
            PredictedRotation.Roll += PredictedAngularVel.Z * PredictionDelta;
            Predicted.Rotation = PredictedRotation.GetNormalized();
        }
        else
        {
            // 회전하지 않을 때는 현재 회전 유지
            Predicted.Rotation = LatestData.Rotation;
        }

        // 디버그 출력 (개발 중에만)
        if (GEngine && GEngine->bEnableOnScreenDebugMessages)
        {
            FString DebugText = FString::Printf(
                TEXT("Moving: %s, Rotating: %s, Stationary: %.1fs, Strength: %.2f"),
                bIsMoving ? TEXT("Yes") : TEXT("No"),
                bIsRotating ? TEXT("Yes") : TEXT("No"),
                StationaryTime,
                PredictionStrength
            );
            GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, DebugText);
        }
    }

    PredictedCamera = Predicted;
    return Predicted;
}

FCameraPredictionData ASSPlayerController::CorrectPredictionWithServerData(
    const FCameraPredictionData& Prediction,
    const FRepCamInfo& ServerData)
{
    FCameraPredictionData Corrected = Prediction;

    // 서버 데이터와 예측 사이의 오차 계산
    FVector LocationError = ServerData.Location - Prediction.Location;
    FRotator RotationError = (ServerData.Rotation - Prediction.Rotation).GetNormalized();

    float LocationErrorMagnitude = LocationError.Size();
    float RotationErrorMagnitude = FMath::Sqrt(
        RotationError.Pitch * RotationError.Pitch +
        RotationError.Yaw * RotationError.Yaw +
        RotationError.Roll * RotationError.Roll
    );

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    // 정지 상태에서는 더 빠른 보정
    float LocationCorrectionSpeed = CorrectionSpeed;
    float RotationCorrectionSpeed = CorrectionSpeed;

    if (StationaryTime > 0.2f) // 정지 상태에서는 더 빠른 보정
    {
        LocationCorrectionSpeed *= 2.0f;
        RotationCorrectionSpeed *= 3.0f; // 회전은 특히 빠르게
    }

    // 위치 보정
    if (LocationErrorMagnitude > 100.0f) // 1미터 이상
    {
        Corrected.Location = ServerData.Location;
        UE_LOG(LogTemp, Warning, TEXT("Large location error: %.1fcm, immediate correction"), LocationErrorMagnitude);
    }
    else if (LocationErrorMagnitude > 5.0f) // 5cm 이상
    {
        Corrected.Location = FMath::VInterpTo(Prediction.Location, ServerData.Location, DeltaTime, LocationCorrectionSpeed);
    }
    else
    {
        // 작은 오차는 서버 데이터 우선
        Corrected.Location = ServerData.Location;
    }

    // 회전 보정 (더 엄격하게)
    if (RotationErrorMagnitude > 5.0f) // 5도 이상
    {
        Corrected.Rotation = ServerData.Rotation;
        UE_LOG(LogTemp, Warning, TEXT("Large rotation error: %.1f°, immediate correction"), RotationErrorMagnitude);
    }
    else if (RotationErrorMagnitude > 1.0f) // 1도 이상
    {
        Corrected.Rotation = FMath::RInterpTo(Prediction.Rotation, ServerData.Rotation, DeltaTime, RotationCorrectionSpeed);
    }
    else
    {
        // 작은 회전 오차는 서버 데이터 우선
        Corrected.Rotation = ServerData.Rotation;
    }

    // FOV는 항상 서버 데이터 사용
    Corrected.FOV = ServerData.FOV;

    return Corrected;
}

void ASSPlayerController::ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
    // 더미 폰 위치/회전 적용
    DummyPawn->SetActorLocation(CameraData.Location);
    DummyPawn->SetActorRotation(CameraData.Rotation);

    // 컨트롤러 회전도 동기화
    if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
    {
        DummyController->SetControlRotation(CameraData.Rotation);
    }

    // 카메라 FOV 적용
    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        Camera->SetFieldOfView(CameraData.FOV);
    }
}

// 디버그용 함수 (선택적으로 사용)
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
    // 받은 원격 플레이어 위치 정보 로그
    // UE_LOG(LogTemp, Log, TEXT("SS Received remote player location: %s"), *Location.ToString());
}

/*
// 추가: PlayerController 정리 함수
void ASSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    if (GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(ClientSyncTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("SS Cleared sync timer for controller: %s"), *GetName());
    }

    // 더미 컨트롤러인 경우 추가 정리
    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy controller %s ending play"), *GetName());
    }

    Super::EndPlay(EndPlayReason);
}


*/