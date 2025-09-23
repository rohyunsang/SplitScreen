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

    if (!NewPlayer->IsLocalController()) // 클라면
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.Owner = NewPlayer; //  클라 전용 Proxy는 Owner를 클라 PC로

        ASSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ClientProxy)
        {
            ClientCamProxies.Add(NewPlayer, ClientProxy);
            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy for %s"), *NewPlayer->GetName());
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

    // 서버 전용 Proxy 생성
    if (!ServerCamProxy)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            Params
        );

        if (ServerCamProxy && HasAuthority())
        {
            ServerCamProxy->SetSourceFromPlayerIndex(0); // 리슨 서버 시점
            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy (ListenServer POV)"));
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
    if (!DummySpectatorPawn || !ServerCamProxy) return;

    // *** 핵심 수정: 명확하게 원격 클라이언트만 타겟으로 설정 ***

    // 원격 클라이언트 찾기 (로컬이 아닌 플레이어)
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Found remote client: %s"), *PC->GetName());
            break;
        }
    }

    if (!RemoteClient)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SS No remote client found for server dummy sync"));
        return;
    }

    // 해당 클라이언트의 Proxy 찾기
    ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (!FoundProxy || !*FoundProxy)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS No camera proxy found for remote client"));
        return;
    }

    ASSCameraViewProxy* ClientProxy = *FoundProxy;
    const FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    // 원격 클라이언트의 폰 위치를 기준으로 더미 폰 동기화
    if (RemoteClient->GetPawn())
    {
        FVector TargetPivot = RemoteClient->GetPawn()->GetActorLocation();

        // 현재 DummyPawn 위치/회전 가져오기
        FVector CurrentLocation = DummySpectatorPawn->GetActorLocation();
        FRotator CurrentRotation = DummySpectatorPawn->GetActorRotation();

        float InterpSpeed = 35.f;
        float RotationInterpSpeed = 45.f;

        // 거리 차이가 크면 바로 스냅
        float Distance = FVector::Dist(CurrentLocation, TargetPivot);
        if (Distance > 500.0f)
        {
            DummySpectatorPawn->SetActorLocation(TargetPivot);
            UE_LOG(LogTemp, Log, TEXT("SS Server dummy snapped to remote client at: %s"), *TargetPivot.ToString());
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
            DummySpectatorPawn->SetActorLocation(NewLocation);
        }

        // 컨트롤러 회전은 클라이언트 카메라 회전으로
        if (DummyPlayerController)
        {
            FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
            FRotator NewControlRotation = FMath::RInterpTo(
                CurrentControlRotation,
                RemoteClientCam.Rotation,
                GetWorld()->GetDeltaSeconds(),
                RotationInterpSpeed
            );
            DummyPlayerController->SetControlRotation(NewControlRotation);
        }
    }
    else
    {
        // 클라이언트 폰이 없으면 카메라 위치 직접 사용
        FVector CurrentLocation = DummySpectatorPawn->GetActorLocation();
        float Distance = FVector::Dist(CurrentLocation, RemoteClientCam.Location);

        if (Distance > 500.0f)
        {
            DummySpectatorPawn->SetActorLocation(RemoteClientCam.Location);
        }
        else
        {
            FVector NewLocation = FMath::VInterpTo(
                CurrentLocation,
                RemoteClientCam.Location,
                GetWorld()->GetDeltaSeconds(),
                35.f
            );
            DummySpectatorPawn->SetActorLocation(NewLocation);
        }

        if (DummyPlayerController)
        {
            FRotator NewControlRotation = FMath::RInterpTo(
                DummyPlayerController->GetControlRotation(),
                RemoteClientCam.Rotation,
                GetWorld()->GetDeltaSeconds(),
                45.f
            );
            DummyPlayerController->SetControlRotation(NewControlRotation);
        }
    }
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // 뷰포트 레이아웃은 엔진이 자동으로 처리
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}