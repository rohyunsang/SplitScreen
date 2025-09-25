// Fill out your copyright notice in the Description page of Project Settings.

#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h" // FPlatformUserId 사용을 위해 추가
#include "TimerManager.h" // GetWorldTimerManager() 사용을 위해
#include "SSCameraViewProxy.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"

ASSGameMode::ASSGameMode()
{
    // 기본 클래스들 설정
    PlayerControllerClass = ASSPlayerController::StaticClass();

    // 더미 스펙테이터 폰 클래스 설정
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

    // *** 클라이언트가 접속했을 때만 클라이언트 전용 Proxy 생성 ***
    if (!NewPlayer->IsLocalController()) // 원격 클라이언트
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = NewPlayer; // 클라이언트 전용 Proxy는 Owner를 클라이언트 PC로 설정

        ASSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ClientProxy)
        {
            ClientProxy->SetOwner(NewPlayer);   // 명시적으로 소유권 세팅
            ClientProxy->SetReplicates(true);   // 복제 활성화
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
            // 정확히 2명일 때만 실행 (중복 방지)
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

    // *** 서버 전용 Proxy 생성 (Owner 없음) ***
    if (!ServerCamProxy)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Owner를 설정하지 않음 - 서버 전용 Proxy

        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ServerCamProxy && HasAuthority())
        {
            ServerCamProxy->SetSourceFromPlayerIndex(0); // 리슨 서버 시점 (PlayerIndex 0)
            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy (ListenServer POV, No Owner)"));
        }
    }

    // 더미 플레이어/컨트롤러 생성
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

    // 현재 로컬 플레이어 수 확인
    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Already have 2+ local players"));
        // return;
    }

    // 더미 로컬 플레이어 생성
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

    // 더미 스펙테이터 폰 생성
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

    // 더미 플레이어 컨트롤러 생성
    DummyPlayerController = GetWorld()->SpawnActor<ASSPlayerController>();
    if (DummyPlayerController)
    {
        // 더미로 표시
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

    // === 원격 클라 찾기 ===
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController()) // 원격 클라만
        {
            RemoteClient = PC;
            break;
        }
    }
    if (!RemoteClient) return;

    // === 해당 클라의 Proxy 가져오기 ===
    ASSCameraViewProxy* ClientProxy = nullptr;
    if (ClientCamProxies.Contains(RemoteClient))
    {
        ClientProxy = ClientCamProxies[RemoteClient];
    }
    if (!ClientProxy) return;

    // === 최신 클라 View 가져오기 ===
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



// === 카메라 예측 시스템 구현 함수들 ===

void ASSGameMode::UpdateClientCameraHistory(const FRepCamInfo& ClientCam)
{
    FCameraPredictionData NewData;
    NewData.Location = ClientCam.Location;
    NewData.Rotation = ClientCam.Rotation;
    NewData.FOV = ClientCam.FOV;
    NewData.Timestamp = GetWorld()->GetTimeSeconds();

    // 속도 계산 (이전 데이터가 있는 경우)
    if (ClientCameraHistory.Num() > 0)
    {
        const FCameraPredictionData& LastData = ClientCameraHistory.Last();
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
    ClientCameraHistory.Add(NewData);

    // 히스토리 크기 제한
    if (ClientCameraHistory.Num() > MaxHistorySize)
    {
        ClientCameraHistory.RemoveAt(0);
    }

    // 마지막 클라이언트 데이터 업데이트
    LastClientCamera = NewData;
}

FCameraPredictionData ASSGameMode::PredictClientCameraMovement()
{
    if (ClientCameraHistory.Num() == 0)
    {
        return PredictedClientCamera; // 데이터가 없으면 이전 예측값 유지
    }

    const FCameraPredictionData& LatestData = ClientCameraHistory.Last();
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float PredictionDelta = FMath::Clamp(CurrentTime - LatestData.Timestamp, 0.0f, MaxPredictionTime);

    FCameraPredictionData Predicted = LatestData;

    if (PredictionDelta > 0.0f && ClientCameraHistory.Num() >= 2)
    {
        // 위치는 속도(+가속도) 기반 예측
        Predicted.Location = LatestData.Location + (LatestData.Velocity * PredictionDelta);

        if (ClientCameraHistory.Num() >= 3)
        {
            const FCameraPredictionData& PrevData = ClientCameraHistory[ClientCameraHistory.Num() - 2];
            const float Den = FMath::Max(LatestData.Timestamp - PrevData.Timestamp, 0.001f);
            const FVector Accel = (LatestData.Velocity - PrevData.Velocity) / Den;
            Predicted.Location += 0.5f * Accel * PredictionDelta * PredictionDelta;
        }

        // 회전 예측 제거: 항상 최신 클라이언트 스냅샷의 회전을 그대로 사용
        Predicted.Rotation = LatestData.Rotation;
    }
    else
    {
        // Δt가 0이거나 히스토리가 부족하면 그대로
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

    // 클라이언트 데이터와 예측 사이의 오차 계산
    FVector LocationError = ClientData.Location - Prediction.Location;
    FRotator RotationError = (ClientData.Rotation - Prediction.Rotation).GetNormalized();

    // 오차가 너무 크면 즉시 보정, 작으면 점진적 보정
    float LocationErrorMagnitude = LocationError.Size();
    float RotationErrorMagnitude = FMath::Abs(RotationError.Yaw) + FMath::Abs(RotationError.Pitch);

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    if (LocationErrorMagnitude > ImmediateCorrectionLocationThreshold) // 1미터 이상 차이나면 즉시 보정
    {
        Corrected.Location = ClientData.Location;
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Large location error detected: %.2f, immediate correction"), LocationErrorMagnitude);
    }
    else
    {
        // 점진적 보정
        Corrected.Location = FMath::VInterpTo(Prediction.Location, ClientData.Location, DeltaTime, CorrectionSpeed);
    }

    if (RotationErrorMagnitude > ImmediateCorrectionRotationThreshold) // 10도 이상 차이나면 즉시 보정
    {
        Corrected.Rotation = ClientData.Rotation;
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Large rotation error detected: %.2f, immediate correction"), RotationErrorMagnitude);
    }
    else
    {
        // 점진적 보정
        Corrected.Rotation = FMath::RInterpTo(Prediction.Rotation, ClientData.Rotation, DeltaTime, CorrectionSpeed);
    }

    // FOV는 즉시 적용 (중요도 낮음)
    Corrected.FOV = ClientData.FOV;

    return Corrected;
}

void ASSGameMode::ApplyPredictedClientCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
    if (!DummyPawn) return;

    // === 위치 적용 (예측/보정 값 사용) ===
    FVector CurrentLocation = DummyPawn->GetActorLocation();
    float Distance = FVector::Dist(CurrentLocation, CameraData.Location);

    if (Distance > 200.0f) // 큰 오차 → 즉시 보정
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
            15.f // 보정 속도
        );
        DummyPawn->SetActorLocation(NewLocation);
    }

    // === 회전 적용 ===
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

    // === FOV 적용 ===
    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        Camera->SetFieldOfView(CameraData.FOV);
    }
}



void ASSGameMode::UpdateSplitScreenLayout()
{
    // 뷰포트 레이아웃은 엔진이 자동으로 처리
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}

// 디버그용 함수
void ASSGameMode::DebugServerCameraPrediction()
{
    if (ClientCameraHistory.Num() > 0)
    {
        const FCameraPredictionData& Latest = ClientCameraHistory.Last();
        UE_LOG(LogTemp, Log, TEXT("SS Server Camera Prediction - Velocity: %s, History Size: %d"),
            *Latest.Velocity.ToString(), ClientCameraHistory.Num());
    }
}