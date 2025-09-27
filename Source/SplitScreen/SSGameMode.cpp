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

    // Proxy 생성 (서버에서만)
    if (HasAuthority() && !NewPlayer->IsLocalController())
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = NewPlayer; // 반드시 Owner를 해당 클라의 PlayerController로 설정

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
            // 정확히 2명일 때만 실행
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
    // 원격 클라 찾기 → 더미 스펙테이터 붙이기
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

    // === 회전 동기화 타이머 시작 ===
    GetWorldTimerManager().SetTimer(
        RotationSyncTimerHandle,   // FTimerHandle 멤버변수 선언 필요
        this,
        &ASSGameMode::SyncDummyRotationWithProxy,
        0.016f,   // 60fps 주기 (16ms)
        true      // 반복
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
        // 더미 폰 스폰
        DummySpectatorPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
            DummySpectatorPawnClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator
        );
    }

    if (DummySpectatorPawn)
    {
        // 클라 캐릭터 스켈레톤 소켓에 Attach
        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        DummySpectatorPawn->AttachToComponent(Mesh, AttachRules, FName("head"));
        // "head" 대신 캐릭터 스켈레톤 소켓 이름 사용

        // Pawn은 보이지 않게 설정
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

void ASSGameMode::SyncDummyRotationWithProxy()
{
    UE_LOG(LogTemp, Warning, TEXT("SS SyncDummyRotationWithProxy called"));

    // 1. 원격 클라 찾기
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

    // 2. 해당 클라의 Proxy 가져오기
    ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (!FoundProxy || !*FoundProxy)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS FoundProxy null"));
        return;
    }

    ASSCameraViewProxy* ClientProxy = *FoundProxy;
    const FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    // 3. 회전만 동기화
    FRotator TargetRot = RemoteClientCam.Rotation;
    FRotator CurrentRot = DummyPlayerController->GetControlRotation();

    // 부드럽게 보간
    FRotator NewRot = FMath::RInterpTo(
        CurrentRot,
        TargetRot,
        GetWorld()->GetDeltaSeconds(),
        45.f // 보간 속도
    );

    DummyPlayerController->SetControlRotation(NewRot);

    UE_LOG(LogTemp, Verbose, TEXT("SS Server: Synced dummy rotation -> %s"), *NewRot.ToString());
}




///////////////////////////////////////////////////////////////////
// 이 밑으론 사용안함. 


void ASSGameMode::SetClientCameraAsSecondView()
{
    // 1. 원격 클라이언트 찾기
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

    // 2. Pawn에서 카메라 찾기
    UCameraComponent* ClientCamera = ClientPawn->FindComponentByClass<UCameraComponent>();
    if (!ClientCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Client pawn has no camera"));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // 3. 두 번째 로컬 플레이어 확보
    ULocalPlayer* SecondPlayer = GameInstance->GetLocalPlayerByIndex(1);
    if (!SecondPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No second local player"));
        return;
    }

    // 4. 두 번째 컨트롤러 생성 (없을 경우)
    if (!SecondPlayer->PlayerController)
    {
        APlayerController* SecondPC = GetWorld()->SpawnActor<APlayerController>();
        SecondPC->SetPlayer(SecondPlayer);
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Created controller for second player"));
    }

    // 5. 두 번째 뷰포트 → 클라 Pawn 카메라로 전환
    if (APlayerController* SecondPC = SecondPlayer->PlayerController)
    {
        FViewTargetTransitionParams TransitionParams;
        SecondPC->SetViewTarget(ClientPawn, TransitionParams); // Pawn에 붙은 CameraComponent 사용

        UE_LOG(LogTemp, Warning, TEXT("SS Server: Second viewport now uses remote client camera"));
    }
}



void ASSGameMode::CreateSimpleDummyLocalPlayer()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // 현재 로컬 플레이어 수 확인
    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Already have 2+ local players"));
        return;
    }

    // 더미 로컬 플레이어 생성 (폰/컨트롤러 없이)
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
    // 원격 클라이언트 찾기
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

    // 이미 카메라가 있는지 확인
    if (AttachedServerCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Camera already attached"));
        return;
    }

    // 새 카메라 컴포넌트 생성
    AttachedServerCamera = NewObject<UCameraComponent>(ClientPawn, UCameraComponent::StaticClass(), TEXT("ServerViewCamera"));

    // 머리 소켓에 붙이기
    FName HeadSocket = TEXT("head");
    if (ClientMesh->DoesSocketExist(HeadSocket))
    {
        AttachedServerCamera->SetupAttachment(ClientMesh, HeadSocket);
    }
    else
    {
        AttachedServerCamera->SetupAttachment(ClientMesh);
        AttachedServerCamera->SetRelativeLocation(FVector(0, 0, 160)); // 머리 높이
    }

    // 카메라 설정 - 회전 동기화 완전 제거!
    AttachedServerCamera->SetFieldOfView(90.0f);
    AttachedServerCamera->bUsePawnControlRotation = false; // 중요: 컨트롤러 회전 사용 안함
    AttachedServerCamera->bAutoActivate = true;

    // 컴포넌트 등록
    AttachedServerCamera->RegisterComponent();

    // 두 번째 뷰포트 설정
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        ULocalPlayer* SecondPlayer = GameInstance->GetLocalPlayerByIndex(1);
        if (SecondPlayer)
        {
            // 두 번째 플레이어용 컨트롤러 생성/설정
            if (!SecondPlayer->PlayerController)
            {
                APlayerController* SecondPC = GetWorld()->SpawnActor<APlayerController>();
                SecondPC->SetPlayer(SecondPlayer);
            }

            // 클라이언트 캐릭터를 직접 뷰 타겟으로 설정
            if (APlayerController* SecondPC = SecondPlayer->PlayerController)
            {
                SecondPC->SetViewTarget(ClientPawn); // 클라이언트 캐릭터를 직접 타겟팅

                UE_LOG(LogTemp, Warning, TEXT("SS Server: Second viewport set to client character with attached camera"));
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Server: Camera attached to client character without rotation sync"));
}

// 이 밑으로 안씀 


void ASSGameMode::SetSecondViewportCamera(UCameraComponent* Camera)
{
    if (!Camera) return;

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // 두 번째 로컬 플레이어 가져오기
    ULocalPlayer* SecondPlayer = GameInstance->GetLocalPlayerByIndex(1);
    if (!SecondPlayer) return;

    // 두 번째 플레이어용 컨트롤러 생성 (없는 경우에만)
    if (!SecondPlayer->PlayerController)
    {
        APlayerController* SecondPC = GetWorld()->SpawnActor<APlayerController>();
        SecondPC->SetPlayer(SecondPlayer);

        UE_LOG(LogTemp, Warning, TEXT("SS Server: Created controller for second player"));
    }

    if (APlayerController* SecondPC = SecondPlayer->PlayerController)
    {
        // 클라이언트 캐릭터를 직접 뷰 타겟으로 설정
        // 이렇게 하면 붙인 카메라가 자동으로 사용됨
        SecondPC->SetViewTarget(Camera->GetOwner());

        UE_LOG(LogTemp, Warning, TEXT("SS Server: Second viewport now views client character directly"));
    }
}


// 아래로는 사용안함 


void ASSGameMode::SyncDummyPlayerWithRemotePlayer()
{
    if (!DummySpectatorPawn || !ServerCamProxy) return;

    // 원격 클라이언트 찾기 (로컬이 아닌 첫 번째 플레이어)
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

    // 해당 클라이언트의 Proxy 찾기
    ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (!FoundProxy || !*FoundProxy)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Server: No camera proxy found for remote client"));
        return;
    }

    ASSCameraViewProxy* ClientProxy = *FoundProxy;
    const FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    // === 예측 시스템 적용 ===

    // 1) 새로운 클라이언트 데이터가 도착했는지 확인
    bool bNewClientData = false;
    if (!LastClientCamera.Location.Equals(RemoteClientCam.Location, 1.0f) ||
        !LastClientCamera.Rotation.Equals(RemoteClientCam.Rotation, 1.0f))
    {
        bNewClientData = true;
        UpdateClientCameraHistory(RemoteClientCam);
        UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: New client camera data - Rotation: %s"),
            *RemoteClientCam.Rotation.ToString());
    }

    // 2) 카메라 위치 예측 수행
    FCameraPredictionData PredictedState = PredictClientCameraMovement();

    // 3) 클라이언트 데이터로 예측 보정 (새 데이터가 있을 때만)
    if (bNewClientData)
    {
        PredictedState = CorrectPredictionWithClientData(PredictedState, RemoteClientCam);
    }

    // 4) 더미 폰에 예측된 카메라 적용
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

    // 원격 클라이언트의 폰 찾기
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
        // 클라이언트 폰 위치를 피벗으로 사용
        FVector TargetPivot = RemoteClient->GetPawn()->GetActorLocation();

        // 현재 더미 폰 위치
        FVector CurrentLocation = DummyPawn->GetActorLocation();
        float InterpSpeed = 35.f;

        // 거리 차이가 크면 바로 스냅
        float Distance = FVector::Dist(CurrentLocation, TargetPivot);
        if (Distance > 500.0f)
        {
            DummyPawn->SetActorLocation(TargetPivot);
            UE_LOG(LogTemp, Log, TEXT("SS Server: Dummy snapped to client at: %s"), *TargetPivot.ToString());
        }
        else
        {
            // 부드럽게 보간 - 클라이언트 폰 위치로
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
        // 클라이언트 폰이 없으면 예측된 카메라 위치 직접 사용
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

    // 컨트롤러 회전은 예측된 클라이언트 카메라 회전으로
    if (DummyPlayerController)
    {
        FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
        FRotator NewControlRotation = FMath::RInterpTo(
            CurrentControlRotation,
            CameraData.Rotation, // 예측된 회전 사용
            GetWorld()->GetDeltaSeconds(),
            45.f
        );
        DummyPlayerController->SetControlRotation(NewControlRotation);

        UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Applied predicted client rotation: %s"),
            *NewControlRotation.ToString());
    }

    // 카메라 FOV 적용
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