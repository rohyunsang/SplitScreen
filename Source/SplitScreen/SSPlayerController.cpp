// Fill out your copyright notice in the Description page of Project Settings.

#include "SSPlayerController.h"
#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "SSDummySpectatorPawn.h"
#include "SSCameraViewProxy.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"

//////////////////////////////////////////////////////////////////////////
// ���� ��ƿ: �Ӱ谨�� ����(1 - exp(-k*dt))
static float CritDampedAlpha(float Gain, float Dt)
{
    return 1.f - FMath::Exp(-Gain * Dt);
}

//////////////////////////////////////////////////////////////////////////
// ctor
ASSPlayerController::ASSPlayerController()
{
    bReplicates = true;
}

//////////////////////////////////////////////////////////////////////////
// BeginPlay
void ASSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!bIsDummyController)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Player Controller Started - IsLocalController: %s"),
            IsLocalController() ? TEXT("true") : TEXT("false"));

        // Ŭ���̾�Ʈ ���� ��Ʈ�ѷ������� ���ø� ����
        if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Client detected - Setting up split screen"));

            if (bClientSplitScreenSetupComplete)
            {
                UE_LOG(LogTemp, Warning, TEXT("SS Client split screen already setup, skipping"));
                return;
            }

            if (USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance()))
            {
                SSGI->EnableSplitScreen();
            }

            // �ణ�� ���� �� ����(�����÷��̾� �غ� �ð�)
            FTimerHandle ClientSetupHandle;
            GetWorldTimerManager().SetTimer(
                ClientSetupHandle,
                [this]()
                {
                    if (!bClientSplitScreenSetupComplete)
                    {
                        SetupClientSplitScreen();
                    }
                },
                2.0f,
                false
            );
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// EndPlay
void ASSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(ClientSyncTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("SS Cleared sync timer for controller: %s"), *GetName());
    }

    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy controller %s ending play"), *GetName());
    }

    Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////////
// �Է�
void ASSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (bIsDummyController)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy Controller - Skipping input setup"));
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
// Tick
void ASSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsDummyController) return;

    // *** ����: ���� ��ġ ������Ʈ RPC ��Ȱ��ȭ ***
    // ���� ��� ī�޶� �����ʹ� ASSCameraViewProxy�� ���� ���� ����ȭ
    /*
    if (IsLocalController() && GetPawn())
    {
        TimeSinceLastUpdate += DeltaTime;
        if (TimeSinceLastUpdate >= LocationUpdateInterval)
        {
            ServerUpdatePlayerLocation(GetPawn()->GetActorLocation(),
                GetPawn()->GetActorRotation());
            TimeSinceLastUpdate = 0.0f;
        }
    }
    */
}

//////////////////////////////////////////////////////////////////////////
// ����/���ø� ����
void ASSPlayerController::SetupClientSplitScreen()
{
    if (bClientSplitScreenSetupComplete)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client split screen setup already complete"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up client split screen"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    const int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();
    UE_LOG(LogTemp, Warning, TEXT("SS Client current local players: %d"), CurrentLocalPlayers);

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client already has 2+ local players"));

        if (!ClientDummyPawn)
        {
            CreateClientDummyPawn();
        }

        bClientSplitScreenSetupComplete = true;
        return;
    }

    // �� ��° ���� �÷��̾� ����
    FPlatformUserId DummyUserId = FPlatformUserId::CreateFromInternalId(1);
    FString OutError;
    if (ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false))
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy local player created successfully"));
        CreateClientDummyPawn();
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

    if (ClientDummyPawn && IsValid(ClientDummyPawn))
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn already exists and is valid"));
        return;
    }

    FVector SpawnLoc(0, 0, 200);
    FRotator SpawnRot = FRotator::ZeroRotator;

    ClientDummyPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
        ASSDummySpectatorPawn::StaticClass(), SpawnLoc, SpawnRot);

    if (!ClientDummyPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create client dummy pawn"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn created successfully"));

    // ���� ���� ��Ʈ�ѷ� Ž��
    ASSPlayerController* DummyController = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASSPlayerController* PC = Cast<ASSPlayerController>(*It);
        if (PC && PC->bIsDummyController && PC != this)
        {
            DummyController = PC;
            UE_LOG(LogTemp, Warning, TEXT("SS Found existing dummy controller: %s"), *PC->GetName());
            break;
        }
    }

    // �ʿ� �� ����/����
    if (!DummyController)
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (GI->GetNumLocalPlayers() >= 2)
            {
                if (ULocalPlayer* LP1 = GI->GetLocalPlayerByIndex(1))
                {
                    if (!LP1->PlayerController)
                    {
                        DummyController = GetWorld()->SpawnActor<ASSPlayerController>();
                        if (DummyController)
                        {
                            DummyController->SetAsDummyController(true);
                            UE_LOG(LogTemp, Warning, TEXT("SS New dummy controller created: %s"), *DummyController->GetName());
                        }
                    }
                    else if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(LP1->PlayerController))
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
        if (UGameInstance* GI = GetGameInstance())
        {
            if (GI->GetNumLocalPlayers() >= 2)
            {
                if (ULocalPlayer* LP1 = GI->GetLocalPlayerByIndex(1))
                {
                    if (!LP1->PlayerController || LP1->PlayerController != DummyController)
                    {
                        DummyController->SetPlayer(LP1);
                    }

                    if (!ClientDummyPawn->GetController() || ClientDummyPawn->GetController() != DummyController)
                    {
                        DummyController->Possess(ClientDummyPawn);
                    }

                    UE_LOG(LogTemp, Warning, TEXT("SS Client dummy controller setup complete"));
                }
            }
        }
    }

    if (!GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
    {
        StartClientDummySync(ClientDummyPawn);
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

    GetWorldTimerManager().SetTimer(
        ClientSyncTimerHandle,
        [this, DummyPawn]()
        {
            SyncClientDummyWithRemotePlayer(DummyPawn);
        },
        0.0167f,   // 60Hz ����
        true
    );
}

//////////////////////////////////////////////////////////////////////////
// ����/���� ����
void ASSPlayerController::SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn)
{
    if (!DummyPawn) return;

    // 1) ���� ī�޶� ���Ͻ� ã��/ĳ��
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

    // *** ����: ���Ͻÿ��� ��ġ�� �Բ� ���������� ���� ***
    // ServerCam�� ��ġ ������ ���ԵǾ� �ִٰ� ����
    // ���� ���ٸ� FRepCamInfo ����ü�� Location �߰� �ʿ�

    // 2) �� ���� ������ ���� -> �����丮 ������Ʈ (��ġ�� ����)
    bool bNewServerData = false;
    if (!LastServerCamera.Location.Equals(ServerCam.Location, 0.5f) ||  // �Ӱ谪 ��ȭ
        !LastServerCamera.Rotation.Equals(ServerCam.Rotation, 0.1f) ||   // �Ӱ谪 ��ȭ
        FMath::Abs(LastServerCamera.FOV - ServerCam.FOV) > KINDA_SMALL_NUMBER)
    {
        bNewServerData = true;
        UpdateCameraHistory(ServerCam);
    }

    // 3) ������ ����/�ʰ��������� ����
    FCameraPredictionData Predicted = PredictCameraMovement();

    // 4) �Ӱ谨�� ����(�� ������ ���� ����)
    if (bNewServerData)
    {
        Predicted = CorrectPredictionWithServerData(Predicted, ServerCam);
    }

    // 5) ����
    ApplyPredictedCamera(DummyPawn, Predicted);
}

void ASSPlayerController::UpdateCameraHistory(const FRepCamInfo& ServerCam)
{
    FCameraPredictionData NewData;
    NewData.Location = ServerCam.Location;
    NewData.Rotation = ServerCam.Rotation;
    NewData.FOV = ServerCam.FOV;
    NewData.Timestamp = GetWorld()->GetTimeSeconds();

    if (CameraHistory.Num() > 0)
    {
        const FCameraPredictionData& Last = CameraHistory.Last();
        const float dt = FMath::Max(NewData.Timestamp - Last.Timestamp, 0.0001f);

        // ���ӵ�
        NewData.Velocity = (NewData.Location - Last.Location) / dt;

        // ���ӵ�(���Ϸ� ��� ��-�� ������� �ٻ��� deg/s)
        const FQuat Q0 = Last.Rotation.Quaternion().GetNormalized();
        const FQuat Q1 = NewData.Rotation.Quaternion().GetNormalized();
        const FQuat D = Q1 * Q0.Inverse();

        FVector Axis; float AngleRad;
        D.ToAxisAndAngle(Axis, AngleRad);
        const float Deg = FMath::RadiansToDegrees(FMath::Abs(AngleRad));
        const float w = Deg / dt;
        NewData.AngularVelocity = FVector(w, w, w); // �ܼ� �α׿�
    }

    CameraHistory.Add(NewData);
    if (CameraHistory.Num() > MaxHistorySize)
    {
        CameraHistory.RemoveAt(0);
    }

    LastServerCamera = NewData;
}

// *** �߰�: �̵� �� ������ ������ ���� �Լ� ***
FCameraPredictionData ASSPlayerController::PredictCameraMovement()
{
    if (CameraHistory.Num() == 0) return PredictedCamera;

    const float Now = GetWorld()->GetTimeSeconds();
    const float RenderTime = Now - InterpDelaySec;

    // RenderTime�� �����ϴ� �ֱ� ���� �ε���
    int32 NewerIdx = INDEX_NONE;
    for (int32 i = CameraHistory.Num() - 1; i >= 0; --i)
    {
        if (CameraHistory[i].Timestamp >= RenderTime)
        {
            NewerIdx = i;
        }
        else
        {
            break;
        }
    }

    auto SlerpRot = [](const FRotator& A, const FRotator& B, float Alpha)
        {
            const FQuat QA = A.Quaternion().GetNormalized();
            const FQuat QB = B.Quaternion().GetNormalized();
            return FQuat::Slerp(QA, QB, Alpha).GetNormalized().Rotator();
        };

    // *** ����: �ӵ� ��� ���� (������� ���) ***
    auto CalcSmoothedVelocity = [this](int32 FromIdx, int32 ToIdx) -> FVector
        {
            if (FromIdx == ToIdx || FromIdx < 0 || ToIdx >= CameraHistory.Num())
                return FVector::ZeroVector;

            FVector TotalVel = FVector::ZeroVector;
            float TotalWeight = 0.f;

            for (int32 i = FromIdx; i < ToIdx; ++i)
            {
                const float dt = FMath::Max(CameraHistory[i + 1].Timestamp - CameraHistory[i].Timestamp, 0.0001f);
                const FVector vel = (CameraHistory[i + 1].Location - CameraHistory[i].Location) / dt;
                const float weight = 1.f / (ToIdx - i); // �ֱ��ϼ��� ���� ����ġ

                TotalVel += vel * weight;
                TotalWeight += weight;
            }

            return TotalWeight > 0.f ? TotalVel / TotalWeight : FVector::ZeroVector;
        };

    // --- ������ ���� ---
    if (NewerIdx != INDEX_NONE && NewerIdx > 0)
    {
        const auto& Newer = CameraHistory[NewerIdx];
        const auto& Older = CameraHistory[NewerIdx - 1];

        const float span = FMath::Max(Newer.Timestamp - Older.Timestamp, 0.0001f);
        const float alpha = FMath::Clamp((RenderTime - Older.Timestamp) / span, 0.f, 1.f);

        FCameraPredictionData Out = Newer;
        Out.Location = FMath::Lerp(Older.Location, Newer.Location, alpha);
        Out.Rotation = SlerpRot(Older.Rotation, Newer.Rotation, alpha);
        PredictedCamera = Out;
        return Out;
    }

    // --- �ʰ����� (�̵� �� ���� ���� ����) ---
    const auto& Latest = CameraHistory.Last();
    if (CameraHistory.Num() >= 3) // �ּ� 3�� ���� �ʿ�
    {
        const float LeadRaw = RenderTime - Latest.Timestamp;
        const float Lead = FMath::Clamp(LeadRaw, 0.f, MaxExtrapolateSec);

        // *** ����: ���� ���� ��� �������� �ӵ� ��� ***
        const int32 SampleCount = FMath::Min(3, CameraHistory.Num() - 1);
        const FVector vLin = CalcSmoothedVelocity(CameraHistory.Num() - 1 - SampleCount, CameraHistory.Num() - 1);

        // ���ӵ� ��� (�ֱ� 2�� ����)
        const auto& Prev = CameraHistory[CameraHistory.Num() - 2];
        const FQuat Q0 = Prev.Rotation.Quaternion().GetNormalized();
        const FQuat Q1 = Latest.Rotation.Quaternion().GetNormalized();
        const FQuat D = Q1 * Q0.Inverse();

        FVector Axis; float AngleRad;
        D.ToAxisAndAngle(Axis, AngleRad);
        const float dt = FMath::Max(Latest.Timestamp - Prev.Timestamp, 0.0001f);
        const float wDeg = FMath::RadiansToDegrees(FMath::Abs(AngleRad)) / dt;

        // *** ����: �̵� �߿��� ������ �� ���������� ***
        const bool bIsMoving = vLin.Size() > LinearDeadzoneCmPerS;
        const float PredictionScale = bIsMoving ? 0.7f : 1.0f; // �̵� �� ���� ���� ����
        const float UsedLead = Lead * PredictionScale;

        // ��ġ ����
        FVector pos = Latest.Location + vLin * UsedLead;

        // ȸ�� ���� (���ӵ� ����)
        const float wRad = FMath::Min(FMath::Abs(AngleRad) / dt,
            FMath::DegreesToRadians(MaxAngularSpeedDeg));
        const FQuat QEx = FQuat((Axis.IsNearlyZero() ? FVector::UpVector : Axis.GetSafeNormal()),
            wRad * UsedLead) * Q1;

        FCameraPredictionData Out = Latest;
        Out.Location = pos;
        Out.Rotation = QEx.GetNormalized().Rotator();
        PredictedCamera = Out;
        return Out;
    }

    // ���� ������ �ֽ� ������ ���
    PredictedCamera = Latest;
    return Latest;
}

// *** ����: ���� ���� ���� ***
FCameraPredictionData ASSPlayerController::CorrectPredictionWithServerData(
    const FCameraPredictionData& Prediction,
    const FRepCamInfo& ServerData)
{
    FCameraPredictionData Corrected = Prediction;

    const float Dt = FMath::Max(GetWorld()->GetDeltaSeconds(), 0.0001f);

    // *** ����: �̵� ���¿� ���� ������ ���� ���� ***
    const FVector PredictedVel = (CameraHistory.Num() >= 2) ?
        CameraHistory.Last().Velocity : FVector::ZeroVector;
    const bool bIsMoving = PredictedVel.Size() > LinearDeadzoneCmPerS;

    // �̵� �߿��� �� �ε巯�� ����
    const float PosGain = bIsMoving ? PosErrorGain * 0.6f : PosErrorGain;
    const float RotGain = bIsMoving ? RotErrorGain * 0.6f : RotErrorGain;

    const float aP = CritDampedAlpha(PosGain, Dt);
    const float aR = CritDampedAlpha(RotGain, Dt);

    // ��ġ: �Ӱ谨���� ���� ����
    Corrected.Location = FMath::Lerp(Prediction.Location, ServerData.Location, aP);

    // ȸ��: ���ʹϾ� Slerp
    const FQuat QPred = Prediction.Rotation.Quaternion().GetNormalized();
    const FQuat QServ = FQuat(ServerData.Rotation).GetNormalized();
    Corrected.Rotation = FQuat::Slerp(QPred, QServ, aR).GetNormalized().Rotator();

    // FOV�� ���
    Corrected.FOV = ServerData.FOV;

    return Corrected;
}

void ASSPlayerController::ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn,
    const FCameraPredictionData& CameraData)
{
    DummyPawn->SetActorLocation(CameraData.Location);
    DummyPawn->SetActorRotation(CameraData.Rotation);

   
    if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
    {
        DummyController->SetControlRotation(CameraData.Rotation);
    }

    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        // ī�޶󿡼� bUsePawnControlRotation=false ����(�������Ʈ/����Ʈ����)
        Camera->SetFieldOfView(CameraData.FOV);
    }
}

void ASSPlayerController::DebugCameraPrediction()
{
    if (CameraHistory.Num() > 0)
    {
        const FCameraPredictionData& Latest = CameraHistory.Last();
        UE_LOG(LogTemp, Log, TEXT("Camera Prediction - v:%s, hist:%d"),
            *Latest.Velocity.ToString(), CameraHistory.Num());
    }
}

//////////////////////////////////////////////////////////////////////////
// RPC ����
void ASSPlayerController::ServerUpdatePlayerLocation_Implementation(FVector Location, FRotator Rotation)
{
    if (ASSGameMode* SSGameMode = Cast<ASSGameMode>(GetWorld()->GetAuthGameMode()))
    {
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
    // �ʿ� �� ����׿�
    // UE_LOG(LogTemp, Log, TEXT("SS Received remote player location: %s"), *Location.ToString());
}

//////////////////////////////////////////////////////////////////////////
// Dummy flag
void ASSPlayerController::SetAsDummyController(bool bDummy)
{
    bIsDummyController = bDummy;
    UE_LOG(LogTemp, Log, TEXT("SS Controller %s set as dummy: %s"),
        *GetName(), bDummy ? TEXT("Yes") : TEXT("No"));
}
