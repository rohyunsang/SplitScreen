// Fill out your copyright notice in the Description page of Project Settings.

#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h" // FPlatformUserId 사용을 위해 추가
#include "TimerManager.h" // GetWorldTimerManager() 사용을 위해
#include "SSCameraViewProxy.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
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

    // === 개선된 ViewTarget 시스템 ===
    // 프록시 시스템은 유지하되, 더미 플레이어는 SetViewTarget 사용

    // 1) 클라이언트별 개별 Proxy 생성 (모든 원격 클라이언트용)
    if (!NewPlayer->IsLocalController()) // 원격 클라이언트
    {
        FActorSpawnParameters ClientProxyParams;
        ClientProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ClientProxyParams.Owner = NewPlayer; // 클라이언트를 Owner로 설정

        ASSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ClientProxyParams
        );

        if (ClientProxy)
        {
            // 중요: 클라이언트에도 복제되도록 설정
            ClientProxy->SetReplicates(true);
            ClientProxy->SetReplicateMovement(false); // 카메라 데이터만 복제

            // 클라이언트별 Proxy 맵에 추가
            ClientCamProxies.Add(NewPlayer, ClientProxy);

            UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy for %s (Owner: %s)"),
                *NewPlayer->GetName(), *ClientProxy->GetOwner()->GetName());
        }
    }

    // 2) 서버 로컬 플레이어용 Proxy 생성 (한 번만)
    if (NewPlayer->IsLocalController() && !ServerCamProxy)
    {
        FActorSpawnParameters ServerProxyParams;
        ServerProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Owner를 설정하지 않음 - 서버 전용 Proxy

        ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
            ASSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ServerProxyParams
        );

        if (ServerCamProxy)
        {
            // 서버 Proxy도 복제되도록 설정
            ServerCamProxy->SetReplicates(true);
            ServerCamProxy->SetReplicateMovement(false);

            UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy (ListenServer POV, No Owner)"));
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

        // 프록시 정리
        if (ClientCamProxies.Contains(PC))
        {
            if (ASSCameraViewProxy* Proxy = ClientCamProxies[PC])
            {
                Proxy->Destroy();
            }
            ClientCamProxies.Remove(PC);
        }
    }

    Super::Logout(Exiting);
}

void ASSGameMode::SetupOnlineSplitScreen()
{
    UE_LOG(LogTemp, Warning, TEXT("SSGameMode::SetupOnlineSplitScreen called"));

    // 더미 로컬 플레이어 생성
    CreateDummyLocalPlayer();

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

    if (RemoteClient && RemoteClient->GetPawn() && DummyPlayerController)
    {
        // === SetViewTarget 방식 사용 ===
        // 더미 플레이어 컨트롤러가 원격 클라이언트의 Pawn을 직접 바라보도록 설정

        // ViewTarget 설정 (블렌드 없이 즉시 전환)
        DummyPlayerController->SetViewTarget(RemoteClient->GetPawn());

        // ViewTargetBlendParams 설정 (선택사항)
        FViewTargetTransitionParams TransitionParams;
        TransitionParams.BlendTime = 0.0f; // 즉시 전환
        DummyPlayerController->SetViewTargetWithBlend(RemoteClient->GetPawn(), 0.0f, EViewTargetBlendFunction::VTBlend_Linear);

        UE_LOG(LogTemp, Warning, TEXT("SS SetViewTarget: DummyController now viewing %s's pawn"),
            *RemoteClient->GetName());

        // === 옵션 1: 카메라 컴포넌트가 있는 경우 직접 사용 ===
        if (UCameraComponent* CameraComp = RemoteClient->GetPawn()->FindComponentByClass<UCameraComponent>())
        {
            // 카메라 컴포넌트가 있으면 자동으로 그것을 사용
            UE_LOG(LogTemp, Warning, TEXT("SS Using existing camera component from client pawn"));
        }
        // === 옵션 2: SpringArm이 있는 경우 ===
        else if (USpringArmComponent* SpringArm = RemoteClient->GetPawn()->FindComponentByClass<USpringArmComponent>())
        {
            // SpringArm의 끝점을 카메라 위치로 사용
            UE_LOG(LogTemp, Warning, TEXT("SS Using SpringArm component for camera view"));
        }
        else
        {
            // === 옵션 3: 카메라가 없으면 폰의 위치에서 오프셋 적용 ===
            UE_LOG(LogTemp, Warning, TEXT("SS No camera component found, using pawn location with offset"));

            // 주기적으로 ViewTarget을 업데이트하는 타이머 설정 (필요시)
            GetWorldTimerManager().SetTimer(
                ViewTargetUpdateTimerHandle,
                this,
                &ASSGameMode::UpdateDummyViewTarget,
                0.016f,  // 60fps
                true
            );
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to setup view target - RemoteClient or DummyController invalid"));
    }

    UE_LOG(LogTemp, Warning, TEXT("SS SetupOnlineSplitScreen completed - Using SetViewTarget"));
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
        // return; // 필요시 주석 해제
    }

    // 더미 로컬 플레이어 생성
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player: %s"), *OutError);
        return;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Successfully created dummy local player"));
    }

    // 더미 플레이어 컨트롤러 생성
    DummyPlayerController = GetWorld()->SpawnActor<ASSPlayerController>();
    if (DummyPlayerController)
    {
        // 더미로 표시
        DummyPlayerController->SetAsDummyController(true);
        DummyPlayerController->SetPlayer(DummyLocalPlayer);

        // 더미 스펙테이터 폰은 필요시에만 생성 (SetViewTarget 방식에서는 선택사항)
        if (DummySpectatorPawnClass)
        {
            DummySpectatorPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
                DummySpectatorPawnClass,
                FVector::ZeroVector,
                FRotator::ZeroRotator
            );

            if (DummySpectatorPawn)
            {
                // 보이지 않게 설정
                DummySpectatorPawn->SetActorHiddenInGame(true);
                DummySpectatorPawn->SetActorEnableCollision(false);

                // 더미 컨트롤러에 possess (선택사항)
                // SetViewTarget을 사용할 경우 possess가 꼭 필요하지 않을 수 있음
                DummyPlayerController->Possess(DummySpectatorPawn);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("SS Dummy Local Player and Controller Created Successfully"));
    }
}

void ASSGameMode::UpdateDummyViewTarget()
{
    // ViewTarget이 유효한지 주기적으로 확인하고 필요시 재설정
    if (!DummyPlayerController)
    {
        return;
    }

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

    if (RemoteClient && RemoteClient->GetPawn())
    {
        // ViewTarget이 변경되었거나 null인 경우 재설정
        if (DummyPlayerController->GetViewTarget() != RemoteClient->GetPawn())
        {
            DummyPlayerController->SetViewTarget(RemoteClient->GetPawn());
            UE_LOG(LogTemp, Warning, TEXT("SS ViewTarget updated to %s's pawn"),
                *RemoteClient->GetName());
        }

        // === 추가적인 카메라 조정이 필요한 경우 ===
        // 프록시에서 카메라 정보를 가져와서 적용할 수도 있음
        if (ASSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient))
        {
            if (*FoundProxy)
            {
                const FRepCamInfo& CamInfo = (*FoundProxy)->GetReplicatedCamera();

                // 필요시 추가적인 카메라 조정
                // 예: DummyPlayerController->SetControlRotation(CamInfo.Rotation);

                UE_LOG(LogTemp, Verbose, TEXT("SS Camera sync - Loc: %s, Rot: %s"),
                    *CamInfo.Location.ToString(), *CamInfo.Rotation.ToString());
            }
        }
    }
}

// 기존의 불필요한 함수들 제거 또는 단순화
void ASSGameMode::AttachDummySpectatorToClient(APlayerController* RemoteClient)
{
    // SetViewTarget 방식을 사용하므로 이 함수는 더 이상 필요 없음
    // 하위 호환성을 위해 빈 함수로 유지하거나 제거
    UE_LOG(LogTemp, Warning, TEXT("SS AttachDummySpectatorToClient is deprecated - using SetViewTarget instead"));
}

void ASSGameMode::SyncDummyRotationWithProxy()
{
    // SetViewTarget 방식을 사용하므로 이 함수는 더 이상 필요 없음
    // UpdateDummyViewTarget으로 대체됨
    UE_LOG(LogTemp, Warning, TEXT("SS SyncDummyRotationWithProxy is deprecated - using UpdateDummyViewTarget instead"));
}