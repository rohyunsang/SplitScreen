// SSPlayerController.cpp

#include "Player/SSPlayerController.h"
#include "Pawn/PartnerSpectatorPawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASSPlayerController::ASSPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true; // 컨트롤러 복제

    // PIP 기본값
    bAutoCreatePartnerView = true;      // ← 반드시 true
    ViewUpdateRate = 0.033f;
    bSmoothPartnerView = true;

    PIPSizeX = 0.30f;  PIPSizeY = 0.30f;
    PIPOriginX = 0.68f; PIPOriginY = 0.02f;
}

void ASSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("SSPlayerController BeginPlay: %s"), *GetNameSafe(this));

    if (IsLocalController())
    {
        if (bAutoCreatePartnerView)
        {
            FTimerHandle InitTimer;
            GetWorld()->GetTimerManager().SetTimer(InitTimer, [this]()
                {
                    if (HasPartner()) CreatePartnerView();
                }, 1.0f, false);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Local Player Controller Initialized"));
        }
    }
}

void ASSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!IsLocalController()) return;

    if (bShowDebugInfo) ShowDebugInfo();

    if (bShowPartnerView && HasPartner())
    {
        ViewUpdateTimer += DeltaTime;
        if (ViewUpdateTimer >= ViewUpdateRate)
        {
            ViewUpdateTimer = 0.f;
            UpdatePartnerView();
        }
    }
}

void ASSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyPartnerView();

    if (PartnerController)
    {
        ClientPartnerDisconnected();
    }

    Super::EndPlay(EndPlayReason);
}

void ASSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("TogglePartnerView", IE_Pressed, this, &ASSPlayerController::OnTogglePartnerView);
    InputComponent->BindAction("ToggleDebug", IE_Pressed, this, &ASSPlayerController::OnToggleDebugInfo);
}

// ======== Rep Notifies ========
void ASSPlayerController::OnRep_PartnerController()
{
    PartnerPawn = PartnerController ? PartnerController->GetPawn() : nullptr;
    bIsConnectedToPartner = (PartnerController != nullptr);

    if (bIsConnectedToPartner && bAutoCreatePartnerView && IsLocalController())
    {
        FTimerHandle H;
        GetWorld()->GetTimerManager().SetTimer(H, [this]()
            {
                CreatePartnerView();
            }, 0.2f, false);
    }
}

void ASSPlayerController::OnRep_PartnerPawn()
{
    // 필요시 확장
}

// ======== Partner API ========
void ASSPlayerController::SetPartner(ASSPlayerController* NewPartner)
{
    PartnerController = NewPartner;

    if (NewPartner)
    {
        PartnerPawn = NewPartner->GetPawn();
        bIsConnectedToPartner = true;

        const FString PartnerName = (NewPartner->PlayerState ? NewPartner->PlayerState->GetPlayerName() : TEXT("Partner"));
        UE_LOG(LogTemp, Log, TEXT("SSPlayerController: Partner set to %s"), *PartnerName);

        // 서버에서 항상 클라 통지
        ClientPartnerConnected(PartnerName);
        ForceNetUpdate();
    }
    else
    {
        PartnerPawn = nullptr;
        bIsConnectedToPartner = false;

        ClientPartnerDisconnected();
        ForceNetUpdate();
    }
}

// ======== PIP lifecycle ========
void ASSPlayerController::CreatePartnerView()
{
    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Warning, TEXT("CreatePartnerView: Not local controller"));
        return;
    }
    if (SecondaryLocalPlayer && PartnerViewPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreatePartnerView: Already created"));
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Error, TEXT("CreatePartnerView: No GameInstance"));
        return;
    }

    FString OutError;
    // 테스트/에디터에선 인덱스 버전이 가장 안전
    SecondaryLocalPlayer = GI->CreateLocalPlayer(/*ControllerId*/1, OutError, false);

    if (!SecondaryLocalPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("CreatePartnerView: CreateLocalPlayer failed: %s"), *OutError);
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    PartnerViewPawn = GetWorld()->SpawnActor<APartnerSpectatorPawn>(
        APartnerSpectatorPawn::StaticClass(),
        FVector::ZeroVector, FRotator::ZeroRotator, Params);

    if (!PartnerViewPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("CreatePartnerView: Spawn spectator pawn failed"));
        GI->RemoveLocalPlayer(SecondaryLocalPlayer);
        SecondaryLocalPlayer = nullptr;
        return;
    }

    // Possess는 다음 프레임에 컨트롤러가 붙을 수 있으니 재시도 방식
    auto TryPossess = [this]()
        {
            if (!SecondaryLocalPlayer || !PartnerViewPawn) return;

            APlayerController* SecondPC = SecondaryLocalPlayer->PlayerController;
            if (!SecondPC)
            {
                FTimerHandle Retry;
                GetWorld()->GetTimerManager().SetTimer(Retry, [this]() { /* 재귀 캡쳐 */ }, 0.0f, false);
                return;
            }

            SecondPC->Possess(PartnerViewPawn);
            SecondPC->SetViewTarget(PartnerViewPawn);
            SetupSecondaryViewport();

            bShowPartnerView = true;
            bPartnerViewReady = true;

            UE_LOG(LogTemp, Log, TEXT("CreatePartnerView: Possessed by secondary PC"));
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Partner View Created"));
        };

    // 첫 호출
    {
        FTimerHandle First;
        GetWorld()->GetTimerManager().SetTimer(First, [TryPossess]() { TryPossess(); }, 0.0f, false);
    }
}

void ASSPlayerController::DestroyPartnerView()
{
    if (!IsLocalController()) return;

    bShowPartnerView = false;
    bPartnerViewReady = false;

    if (PartnerViewPawn)
    {
        PartnerViewPawn->Destroy();
        PartnerViewPawn = nullptr;
    }

    if (SecondaryLocalPlayer)
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            GI->RemoveLocalPlayer(SecondaryLocalPlayer);
        }
        SecondaryLocalPlayer = nullptr;
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("Partner View Destroyed"));
}

void ASSPlayerController::TogglePartnerView()
{
    if (bShowPartnerView) DestroyPartnerView();
    else                  CreatePartnerView();
}

void ASSPlayerController::SetPartnerViewVisible(bool bVisible)
{
    if (bVisible && !bShowPartnerView) CreatePartnerView();
    else if (!bVisible && bShowPartnerView) DestroyPartnerView();
}

void ASSPlayerController::SetPIPPosition(float OriginX, float OriginY)
{
    PIPOriginX = FMath::Clamp(OriginX, 0.f, 1.f);
    PIPOriginY = FMath::Clamp(OriginY, 0.f, 1.f);

    if (SecondaryLocalPlayer)
    {
        SecondaryLocalPlayer->Origin = FVector2D(PIPOriginX, PIPOriginY);
    }
}

void ASSPlayerController::SetPIPSize(float SizeX, float SizeY)
{
    PIPSizeX = FMath::Clamp(SizeX, 0.1f, 1.f);
    PIPSizeY = FMath::Clamp(SizeY, 0.1f, 1.f);

    if (SecondaryLocalPlayer)
    {
        SecondaryLocalPlayer->Size = FVector2D(PIPSizeX, PIPSizeY);
    }
}

void ASSPlayerController::SetupSecondaryViewport()
{
    if (!SecondaryLocalPlayer) return;

    if (UGameViewportClient* VC = GetWorld()->GetGameViewport())
    {
        VC->SetForceDisableSplitscreen(false);   // 강제 비활성화 해제
        VC->UpdateActiveSplitscreenType();       // 엔진 내부 상태 갱신
    }

    //  직접 PIP 위치 지정
    SecondaryLocalPlayer->Size = FVector2D(PIPSizeX, PIPSizeY);
    SecondaryLocalPlayer->Origin = FVector2D(PIPOriginX, PIPOriginY);
    SecondaryLocalPlayer->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;

    // 메인 플레이어는 전체 화면
    if (ULocalPlayer* Main = GetLocalPlayer())
    {
        Main->Size = FVector2D(1.f, 1.f);
        Main->Origin = FVector2D(0.f, 0.f);
    }
}


// ======== View Sync ========
void ASSPlayerController::UpdatePartnerView()
{
    if (!bShowPartnerView || !GetPawn()) return;

    FPartnerViewData ViewData = GetLocalCameraData();
    if (ViewData.bIsValid) ServerUpdateCameraView(ViewData);
}

FPartnerViewData ASSPlayerController::GetLocalCameraData() const
{
    FPartnerViewData D;
    if (!PlayerCameraManager || !GetPawn()) return D;

    D.CameraLocation = PlayerCameraManager->GetCameraLocation();
    D.CameraRotation = PlayerCameraManager->GetCameraRotation();
    D.CameraFOV = PlayerCameraManager->GetFOVAngle();

    if (const APawn* MyPawn = GetPawn())
    {
        if (const USpringArmComponent* Arm = MyPawn->FindComponentByClass<USpringArmComponent>())
        {
            D.CameraArmLength = Arm->TargetArmLength;
        }
    }
    if (PlayerState) D.PlayerID = PlayerState->GetPlayerId();

    D.bIsValid = true;
    return D;
}

void ASSPlayerController::ShowDebugInfo()
{
    if (!GEngine) return;
    const FString Text = FString::Printf(
        TEXT("Partner Debug\n- HasPartner: %s\n- ViewActive: %s\n- Ready: %s\n- Secondary: %s\n- ViewPawn: %s"),
        HasPartner() ? TEXT("Yes") : TEXT("No"),
        bShowPartnerView ? TEXT("Yes") : TEXT("No"),
        bPartnerViewReady ? TEXT("Yes") : TEXT("No"),
        SecondaryLocalPlayer ? TEXT("Valid") : TEXT("NULL"),
        PartnerViewPawn ? TEXT("Valid") : TEXT("NULL"));
    GEngine->AddOnScreenDebugMessage(100, 0.f, FColor::Yellow, Text);
}

void ASSPlayerController::OnTogglePartnerView()
{
    if (!HasPartner())
    {
        // 늦복제 대비: 서버에 현재 페어 정보 재통지 요청
        ServerRequestPartner();
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, TEXT("No partner yet. Requesting from server..."));
        return;
    }
    TogglePartnerView();
}

void ASSPlayerController::OnToggleDebugInfo()
{
    bShowDebugInfo = !bShowDebugInfo;
}

// ======== Networking ========
void ASSPlayerController::ServerUpdateCameraView_Implementation(const FPartnerViewData& ViewData)
{
    if (PartnerController)
    {
        PartnerController->ClientReceivePartnerView(ViewData);
    }
}

void ASSPlayerController::ServerRequestPartner_Implementation()
{
    if (PartnerController)
    {
        const FString Name = (PartnerController->PlayerState ? PartnerController->PlayerState->GetPlayerName() : TEXT("Partner"));
        ClientPartnerConnected(Name);
        if (bAutoCreatePartnerView) ClientEnablePartnerView();
    }
}

void ASSPlayerController::ClientReceivePartnerView_Implementation(const FPartnerViewData& ViewData)
{
    if (!PartnerViewPawn || !ViewData.bIsValid || !bShowPartnerView) return;

    if (bSmoothPartnerView)
    {
        PartnerViewPawn->UpdateFromPartnerSmooth(ViewData.CameraLocation, ViewData.CameraRotation, ViewData.CameraArmLength);
    }
    else
    {
        PartnerViewPawn->UpdateFromPartner(ViewData.CameraLocation, ViewData.CameraRotation, ViewData.CameraArmLength);
    }
    PartnerViewPawn->SetCameraFOV(ViewData.CameraFOV);
}

void ASSPlayerController::ClientEnablePartnerView_Implementation()
{
    if (!bAutoCreatePartnerView) return;

    FTimerHandle H;
    GetWorld()->GetTimerManager().SetTimer(H, [this]() { CreatePartnerView(); }, 0.2f, false);
}

void ASSPlayerController::ClientDisablePartnerView_Implementation()
{
    DestroyPartnerView();
}

void ASSPlayerController::ClientPartnerConnected_Implementation(const FString& PartnerName)
{
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("Connected to partner: %s"), *PartnerName));
}

void ASSPlayerController::ClientPartnerDisconnected_Implementation()
{
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Partner disconnected"));
    DestroyPartnerView();
}

void ASSPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASSPlayerController, PartnerController);
    DOREPLIFETIME(ASSPlayerController, PartnerPawn);
}
