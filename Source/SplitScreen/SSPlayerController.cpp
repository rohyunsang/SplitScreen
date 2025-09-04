// Fill out your copyright notice in the Description page of Project Settings.


#include "SSPlayerController.h"
#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "SSDummySpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "HAL/PlatformMisc.h"

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
        0.033f, // 30fps
        true
    );
}

void ASSPlayerController::SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn)
{
    if (!DummyPawn) return;

    // 방법 1: PlayerController를 통해 Pawn 찾기
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC != this && !PC->IsLocalController())
        {
            APawn* RemotePawn = PC->GetPawn();
            if (RemotePawn && RemotePawn != DummyPawn)
            {
                // 원격 플레이어의 카메라 찾기
                UCameraComponent* RemoteCamera = RemotePawn->FindComponentByClass<UCameraComponent>();
                if (RemoteCamera)
                {
                    // 카메라 위치와 회전 동기화
                    FVector TargetLocation = RemoteCamera->GetComponentLocation();
                    FRotator TargetRotation = RemoteCamera->GetComponentRotation();

                    FVector CurrentLocation = DummyPawn->GetActorLocation();
                    FRotator CurrentRotation = DummyPawn->GetActorRotation();

                    float InterpSpeed = 15.0f;
                    float Distance = FVector::Dist(CurrentLocation, TargetLocation);

                    if (Distance > 500.0f)
                    {
                        DummyPawn->SetActorLocation(TargetLocation);
                        DummyPawn->SetActorRotation(TargetRotation);
                    }
                    else
                    {
                        FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, GetWorld()->GetDeltaSeconds(), InterpSpeed);
                        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), InterpSpeed);

                        DummyPawn->SetActorLocation(NewLocation);
                        DummyPawn->SetActorRotation(NewRotation);
                    }
                }
                break; // 첫 번째 원격 플레이어만 사용
            }
        }
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