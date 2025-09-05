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

        // 기존 더미 컨트롤러가 있는지 체크
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

        // 기존 더미 컨트롤러가 없다면 새로 생성
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
            // 두 번째 LocalPlayer 가져오기
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

        // 클라이언트 동기화 시작 (한 번만)
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

    // 1) 프록시를 못 찾았다면 한 번만 스캔해서 캐시
    ASSCameraViewProxy* Proxy = CachedProxy.Get();
    if (!Proxy)
    {
        for (TActorIterator<ASSCameraViewProxy> It(GetWorld()); It; ++It)
        {
            Proxy = *It;
            break; // 첫 번째만 사용
        }
        if (!Proxy) return;
        CachedProxy = Proxy;
        UE_LOG(LogTemp, Warning, TEXT("SS Cached ServerCamProxy on client"));
    }

    // 2) 복제된 서버 화면 시점(위치/회전/FOV) 가져오기
    const FRepCamInfo& View = Proxy->GetReplicatedCamera();

    // 데이터 검증 추가
    if (View.Location.ContainsNaN() || View.Rotation.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Invalid camera data received, skipping update"));
        return;
    }

    // 너무 큰 변화량 체크
    const float MaxLocationChange = 2000.0f; // 프레임당 최대 이동 거리
    const float Dist = FVector::Dist(DummyPawn->GetActorLocation(), View.Location);

    if (Dist > MaxLocationChange)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Camera jump detected (%.2f units), smoothing transition"), Dist);
        // 큰 변화는 여러 프레임에 걸쳐 보간
        const float Dt = GetWorld()->GetDeltaSeconds();
        const FVector NewLocation = FMath::VInterpTo(DummyPawn->GetActorLocation(), View.Location, Dt, 20.0f);
        const FRotator NewRotation = FMath::RInterpTo(DummyPawn->GetActorRotation(), View.Rotation, Dt, 30.0f);

        DummyPawn->SetActorLocation(NewLocation);
        DummyPawn->SetActorRotation(NewRotation);

        // 컨트롤러 회전도 함께 보간
        if (APlayerController* DPC = Cast<APlayerController>(DummyPawn->GetController()))
        {
            const FRotator NewCtrlRot = FMath::RInterpTo(DPC->GetControlRotation(), View.Rotation, Dt, 30.0f);
            DPC->SetControlRotation(NewCtrlRot);
        }

        // FOV도 보간
        if (UCameraComponent* Cam = DummyPawn->FindComponentByClass<UCameraComponent>())
        {
            const float CurrentFOV = Cam->FieldOfView;
            const float NewFOV = FMath::FInterpTo(CurrentFOV, View.FOV, Dt, 10.0f);
            Cam->SetFieldOfView(NewFOV);
        }

        return; // 큰 변화 처리 후 리턴
    }

    // 3) 기존 보간 로직 - 일반적인 움직임 처리
    const float Dt = GetWorld()->GetDeltaSeconds();
    const float MoveSpd = 35.0f, RotSpd = 45.0f;

    // 거리에 따른 처리 분기
    if (Dist > 500.f)
    {
        // 중간 거리 - 즉시 이동하지만 회전은 보간
        DummyPawn->SetActorLocation(View.Location);
        DummyPawn->SetActorRotation(FMath::RInterpTo(DummyPawn->GetActorRotation(), View.Rotation, Dt, RotSpd));
    }
    else
    {
        // 가까운 거리 - 부드러운 보간
        DummyPawn->SetActorLocation(FMath::VInterpTo(DummyPawn->GetActorLocation(), View.Location, Dt, MoveSpd));
        DummyPawn->SetActorRotation(FMath::RInterpTo(DummyPawn->GetActorRotation(), View.Rotation, Dt, RotSpd));
    }

    // 4) 더미 컨트롤러의 ControlRotation도 보간
    if (APlayerController* DPC = Cast<APlayerController>(DummyPawn->GetController()))
    {
        const FRotator NewCtrlRot = FMath::RInterpTo(DPC->GetControlRotation(), View.Rotation, Dt, RotSpd);
        DPC->SetControlRotation(NewCtrlRot);
    }

    // 5) FOV 동기화 (더미 카메라가 컨트롤러 회전을 사용하도록 설정돼 있어야 함)
    if (UCameraComponent* Cam = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        const float CurrentFOV = Cam->FieldOfView;
        const float NewFOV = FMath::FInterpTo(CurrentFOV, View.FOV, Dt, 5.0f); // 부드러운 FOV 전환
        Cam->SetFieldOfView(NewFOV);
        // 필요 시 PostProcess 등 추가 동기화 가능
    }

    // 6) 디버깅용 로그 (큰 변화 감지)
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
void ASSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 타이머 정리
    if (GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(ClientSyncTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

*/