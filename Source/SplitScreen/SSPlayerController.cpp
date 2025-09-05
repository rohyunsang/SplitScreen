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

            // GameInstance에서 스플릿 스크린 활성화
            if (USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance()))
            {
                SSGI->EnableSplitScreen();
            }

            // 더미 로컬 플레이어 생성
            FTimerHandle ClientSetupHandle;
            GetWorldTimerManager().SetTimer(
                ClientSetupHandle,
                [this]()
                {
                    SetupClientSplitScreen();
                },
                2.0f, // 2초 지연
                false
            );
        }
    }
}

void ASSPlayerController::SetupClientSplitScreen()
{
    UE_LOG(LogTemp, Warning, TEXT("SS Setting up client split screen"));

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();
    UE_LOG(LogTemp, Warning, TEXT("SS Client current local players: %d"), CurrentLocalPlayers);

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client already has 2+ local players"));
        // 이미 LocalPlayer가 있다면 더미 폰만 생성
        CreateClientDummyPawn();
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
    if (ClientDummyPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn already exists"));
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

        // 더미 컨트롤러 생성 및 연결
        ASSPlayerController* DummyController = GetWorld()->SpawnActor<ASSPlayerController>();
        if (DummyController)
        {
            DummyController->SetAsDummyController(true);

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

        // 클라이언트 동기화 시작
        StartClientDummySync(ClientDummyPawn);
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

    // 3) 더미 스펙테이터 이동/회전 보간 (GameMode와 동일 룰)
    const float Dt = GetWorld()->GetDeltaSeconds();
    const float MoveSpd = 35.0f, RotSpd = 45.0f;

    const float Dist = FVector::Dist(DummyPawn->GetActorLocation(), View.Location);
    if (Dist > 500.f)
    {
        DummyPawn->SetActorLocation(View.Location);
        DummyPawn->SetActorRotation(View.Rotation);
    }
    else
    {
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
        Cam->SetFieldOfView(View.FOV);
        // 필요 시 PostProcess 등 추가 동기화 가능
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