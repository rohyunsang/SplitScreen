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
            // 선형 속도 계산
            NewData.Velocity = (NewData.Location - LastData.Location) / DeltaTime;

            // 각속도 계산 (단순화된 버전)
            FRotator DeltaRotation = (NewData.Rotation - LastData.Rotation).GetNormalized();
            NewData.AngularVelocity = FVector(DeltaRotation.Pitch, DeltaRotation.Yaw, DeltaRotation.Roll) / DeltaTime;
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
        return PredictedCamera; // 데이터가 없으면 이전 예측값 유지
    }

    const FCameraPredictionData& LatestData = CameraHistory.Last();
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float PredictionDelta = FMath::Clamp(CurrentTime - LatestData.Timestamp, 0.0f, MaxPredictionTime);

    FCameraPredictionData Predicted = LatestData;

    if (PredictionDelta > 0.0f && CameraHistory.Num() >= 2)
    {
        // 위치는 기존처럼 '속도(+가속도)' 기반 예측
        Predicted.Location = LatestData.Location + (LatestData.Velocity * PredictionDelta);

        if (CameraHistory.Num() >= 3)
        {
            const FCameraPredictionData& PrevData = CameraHistory[CameraHistory.Num() - 2];
            const float Den = FMath::Max(LatestData.Timestamp - PrevData.Timestamp, 0.001f);
            const FVector Accel = (LatestData.Velocity - PrevData.Velocity) / Den;
            Predicted.Location += 0.5f * Accel * PredictionDelta * PredictionDelta;
        }

        // [변경] 회전 예측 제거: 각속도 적용 안 함
        //       → 항상 최신 서버 스냅샷의 회전을 그대로 사용
        Predicted.Rotation = LatestData.Rotation;
    }
    else
    {
        // Δt가 0이거나 히스토리가 부족하면 그대로
        Predicted.Location = LatestData.Location;
        Predicted.Rotation = LatestData.Rotation; // [변경] 회전 예측 없음
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

    // 서버 데이터와 예측 사이의 오차 계산
    FVector LocationError = ServerData.Location - Prediction.Location;
    FRotator RotationError = (ServerData.Rotation - Prediction.Rotation).GetNormalized();

    // 오차가 너무 크면 즉시 보정, 작으면 점진적 보정
    float LocationErrorMagnitude = LocationError.Size();
    float RotationErrorMagnitude = FMath::Abs(RotationError.Yaw) + FMath::Abs(RotationError.Pitch);

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    if (LocationErrorMagnitude > 100.0f) // 1미터 이상 차이나면 즉시 보정
    {
        Corrected.Location = ServerData.Location;
        UE_LOG(LogTemp, Warning, TEXT("Large location error detected: %.2f, immediate correction"), LocationErrorMagnitude);
    }
    else
    {
        // 점진적 보정
        Corrected.Location = FMath::VInterpTo(Prediction.Location, ServerData.Location, DeltaTime, CorrectionSpeed);
    }

    if (RotationErrorMagnitude > 10.0f) // 10도 이상 차이나면 즉시 보정
    {
        Corrected.Rotation = ServerData.Rotation;
        UE_LOG(LogTemp, Warning, TEXT("Large rotation error detected: %.2f, immediate correction"), RotationErrorMagnitude);
    }
    else
    {
        // 점진적 보정
        Corrected.Rotation = FMath::RInterpTo(Prediction.Rotation, ServerData.Rotation, DeltaTime, CorrectionSpeed);
    }

    // FOV는 즉시 적용 (중요도 낮음)
    Corrected.FOV = ServerData.FOV;

    return Corrected;
}

void ASSPlayerController::ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
    // 오직 클라에서 서버 캐릭터 예측에만 쓰이니..
    for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
    {
        ACharacter* TargetCharacter = *It;
        if (!TargetCharacter || TargetCharacter->IsLocallyControlled())
            continue;

        // 피벗(더미 폰)을 타겟 위치로
        const FVector Pivot = TargetCharacter->GetActorLocation(); // 필요시 머리 높이 보정
        DummyPawn->SetActorLocation(Pivot);

        // 컨트롤러 회전을 예측값으로 → 스프링암이 그 회전을 받아서 원궤도
        if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
        {
            DummyController->SetControlRotation(CameraData.Rotation);
        }
        break;
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

    /*
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

    */
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