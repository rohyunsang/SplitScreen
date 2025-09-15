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
#include "GameFramework/SpringArmComponent.h"

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

    // 프록시 생성 및 설정
    SetupCameraProxies();

    // 성공한 경우에만 동기화 시작
    if (DummyPlayerController && DummySpectatorPawn && ClientCamProxy)
    {
        GetWorldTimerManager().SetTimer(
            SyncTimerHandle,
            [this]() { SyncDummyPlayerWithProxy(); },
            0.0167f,
            true
        );
        UE_LOG(LogTemp, Warning, TEXT("SS Split screen setup completed successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Split screen setup failed"));
    }
}

void ASSGameMode::SetupCameraProxies()
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // ConnectedPlayers에서 실제 플레이어 찾기
    APlayerController* HostPC = nullptr;
    APlayerController* ClientPC = nullptr;

    for (APlayerController* PC : ConnectedPlayers)
    {
        if (!PC || PC == DummyPlayerController) continue;

        if (PC->IsLocalController())
        {
            HostPC = PC; // 리슨서버의 로컬 컨트롤러
        }
        else
        {
            ClientPC = PC; // 원격 클라이언트
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Found Host PC: %s, Client PC: %s"),
        HostPC ? *HostPC->GetName() : TEXT("NULL"),
        ClientPC ? *ClientPC->GetName() : TEXT("NULL"));

    // 1) 서버 카메라 프록시 (호스트용)
    if (!ServerCamProxy)
    {
        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
        if (ServerCamProxy && HasAuthority() && HostPC)
        {
            ServerCamProxy->SetSourcePC(HostPC); // 직접 PC 지정
            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy with Host PC"));
        }
    }

    // 2) 클라이언트 카메라 프록시 (클라이언트용)
    if (!ClientCamProxy)
    {
        ClientCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
        if (ClientCamProxy && HasAuthority() && ClientPC)
        {
            ClientCamProxy->SetSourcePC(ClientPC); // 직접 PC 지정
            CachedClientProxy = ClientCamProxy;
            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy with Client PC"));
        }
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
    FVector SpawnLocation = FVector(0, 0, 0);
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

void ASSGameMode::SyncDummyPlayerWithProxy()
{
    if (!DummySpectatorPawn) return;

    // 캐시된 프록시 사용 (성능 최적화)
    ASSCameraViewProxy* Proxy = CachedClientProxy.Get();
    if (!Proxy)
    {
        // 캐시가 없으면 ClientCamProxy 사용
        Proxy = ClientCamProxy;
        if (Proxy)
        {
            CachedClientProxy = Proxy;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS ClientCamProxy not found"));
            return;
        }
    }

    // 클라이언트 카메라 데이터를 프록시에서 가져오기
    const FRepCamInfo& ClientCam = Proxy->GetReplicatedCamera();

    // 프록시 데이터 적용
    ApplyProxyCamera(DummySpectatorPawn, ClientCam);
}

void ASSGameMode::ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData)
{
    if (!DummyPawn) return;

    // 클라이언트 캐릭터 위치 찾기
    APlayerController* ClientPC = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController() && PC != DummyPlayerController)
        {
            ClientPC = PC;
            break;
        }
    }

    if (ClientPC && ClientPC->GetPawn())
    {
        // 타겟 캐릭터 위치
        FVector TargetCharacterLocation = ClientPC->GetPawn()->GetActorLocation();
        FVector CurrentLocation = DummyPawn->GetActorLocation();

        // 보간 설정
        float LocationInterpSpeed = 15.0f;  // 위치 보간 속도 (낮춤)
        float RotationInterpSpeed = 25.0f;  // 회전 보간 속도 (낮춤)

        // 거리 체크 - 너무 멀면 즉시 이동
        float Distance = FVector::Dist(CurrentLocation, TargetCharacterLocation);
        if (Distance > 300.0f) // 3미터 이상 차이나면 즉시 이동
        {
            DummyPawn->SetActorLocation(TargetCharacterLocation);
            if (DummyPlayerController)
            {
                DummyPlayerController->SetControlRotation(CamData.Rotation);
            }
            UE_LOG(LogTemp, Warning, TEXT("SS Large character distance detected: %.2f, immediate correction"), Distance);
        }
        else
        {
            // 부드러운 보간
            FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetCharacterLocation, GetWorld()->GetDeltaSeconds(), LocationInterpSpeed);
            DummyPawn->SetActorLocation(NewLocation);

            // 컨트롤러 회전 보간
            if (DummyPlayerController)
            {
                FRotator CurrentControlRotation = DummyPlayerController->GetControlRotation();
                FRotator NewControlRotation = FMath::RInterpTo(CurrentControlRotation, CamData.Rotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);
                DummyPlayerController->SetControlRotation(NewControlRotation);
            }
        }
    }

    // FOV 적용
    if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
    {
        Camera->SetFieldOfView(CamData.FOV);
    }
}

void ASSGameMode::UpdateSplitScreenLayout()
{
    // 뷰포트 레이아웃은 엔진이 자동으로 처리
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}