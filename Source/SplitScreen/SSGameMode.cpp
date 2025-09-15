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
    USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance());
    if (!SSGI || !SSGI->IsSplitScreenEnabled())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen not enabled in game instance"));
        return;
    }

    // 이미 완전히 설정되어 있으면 리턴
    if (DummyPlayerController && DummySpectatorPawn &&
        DummyPlayerController->GetLocalPlayer())
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen already fully setup"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Setting up online split screen..."));

    CreateDummyLocalPlayer();
    UpdateSplitScreenLayout();

    // 성공한 경우에만 동기화 시작
    if (DummyPlayerController && DummySpectatorPawn)
    {
        GetWorldTimerManager().SetTimer(
            SyncTimerHandle,
            [this]() { SyncDummyPlayerWithRemotePlayer(); },
            0.0083f,
            true
        );
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen setup completed successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Split screen setup failed"));
    }

    // 1) 프록시가 없으면 생성
    if (!ServerCamProxy)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
        UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy"));
    }

    // 2) 서버 로컬 플레이어(리슨 서버)의 카메라를 소스로 지정
    if (ServerCamProxy && HasAuthority())
    {
        // 0번 인덱스 PC = 리슨서버의 화면
        ServerCamProxy->SetSourceFromPlayerIndex(0);
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
    if (!DummySpectatorPawn || ConnectedPlayers.Num() < 2) return;

    // 원격 플레이어 찾기
    APlayerController* RemotePlayer = nullptr;
    for (int32 i = 1; i < ConnectedPlayers.Num(); i++)
    {
        if (ConnectedPlayers[i] && ConnectedPlayers[i] != DummyPlayerController)
        {
            RemotePlayer = ConnectedPlayers[i];
            break;
        }
    }

    if (!RemotePlayer || !RemotePlayer->GetPawn()) return;

    APawn* RemotePawn = RemotePlayer->GetPawn();
    UCameraComponent* RemoteCamera = RemotePawn->FindComponentByClass<UCameraComponent>();

    if (RemoteCamera && DummySpectatorPawn->bSyncDirectlyToCamera)
    {
        // 목표 위치와 회전
        FVector TargetLocation = RemoteCamera->GetComponentLocation();
        FRotator TargetRotation = RemoteCamera->GetComponentRotation();

        // 현재 위치와 회전
        FVector CurrentLocation = DummySpectatorPawn->GetActorLocation();
        FRotator CurrentRotation = DummySpectatorPawn->GetActorRotation();

        // 보간 속도 (값이 클수록 빠르게 따라감)
        float InterpSpeed = 35.0f; // 조정 가능
        float RotationInterpSpeed = 45.0f; // 회전은 조금 더 느리게

        // 거리 체크 - 너무 멀면 즉시 이동
        float Distance = FVector::Dist(CurrentLocation, TargetLocation);
        if (Distance > 500.0f) // 5미터 이상 차이나면 즉시 이동
        {
            DummySpectatorPawn->SetActorLocation(TargetLocation);
            DummySpectatorPawn->SetActorRotation(TargetRotation);
        }
        else
        {
            // 부드럽게 보간
            FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, GetWorld()->GetDeltaSeconds(), InterpSpeed);
            FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);

            DummySpectatorPawn->SetActorLocation(NewLocation);
            DummySpectatorPawn->SetActorRotation(NewRotation);
        }

        // 컨트롤러 회전도 보간
        if (DummyPlayerController)
        {
            FRotator ControlRotation = RemotePlayer->GetControlRotation();
            FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
            FRotator NewControlRotation = FMath::RInterpTo(CurrentControlRotation, ControlRotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);
            DummyPlayerController->SetControlRotation(NewControlRotation);
        }
    }
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // 뷰포트 레이아웃은 엔진이 자동으로 처리
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}