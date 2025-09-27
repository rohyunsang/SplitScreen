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
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h" //

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

    if (HasAuthority() && !NewPlayer->IsLocalController())
    {
        if (APawn* NewPawn = NewPlayer->GetPawn())
        {
            // 캐릭터 Mesh 찾기
            USkeletalMeshComponent* MeshComp = NewPawn->FindComponentByClass<USkeletalMeshComponent>();
            if (MeshComp)
            {
                // CameraBoom 생성
                USpringArmComponent* CameraBoom = NewObject<USpringArmComponent>(NewPawn, USpringArmComponent::StaticClass(), TEXT("CameraBoom"));
                if (CameraBoom)
                {
                    CameraBoom->SetupAttachment(MeshComp);
                    CameraBoom->TargetArmLength = 300.f;
                    CameraBoom->bUsePawnControlRotation = true;
                    CameraBoom->RegisterComponent();

                    // 실제 카메라 생성
                    UCameraComponent* FollowCamera = NewObject<UCameraComponent>(NewPawn, UCameraComponent::StaticClass(), TEXT("FollowCamera"));
                    if (FollowCamera)
                    {
                        FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
                        FollowCamera->bUsePawnControlRotation = false;
                        FollowCamera->FieldOfView = 90.f;
                        FollowCamera->RegisterComponent();

                        UE_LOG(LogTemp, Warning, TEXT("SSGameMode: Attached CameraBoom + Camera to %s"), *NewPawn->GetName());
                    }
                }

                SetupOnlineSplitScreen();
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("SSGameMode: No SkeletalMesh found in Pawn %s"), *NewPawn->GetName());
            }
        }
    }
}


void ASSGameMode::SpawnAndAttachCameraProxy(APlayerController* OwnerPC, APawn* OwnerPawn)
{
    if (!OwnerPC || !OwnerPawn) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = OwnerPC;

    ASSCameraViewProxy* CamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
        ASSCameraViewProxy::StaticClass(),
        OwnerPawn->GetActorLocation(),
        OwnerPawn->GetActorRotation(),
        Params
    );

    if (CamProxy)
    {
        ClientCamProxies.Add(OwnerPC, CamProxy);

        // Pawn에 붙여서 이동 자동 동기화
        CamProxy->AttachToActor(OwnerPawn, FAttachmentTransformRules::KeepRelativeTransform);

        UE_LOG(LogTemp, Warning, TEXT("SS Spawned CameraProxy for %s attached to %s"),
            *OwnerPC->GetName(), *OwnerPawn->GetName());
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

    if (USSGameInstance* GI = GetGameInstance<USSGameInstance>())
    {
        GI->EnableSplitScreen();
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

void ASSGameMode::BufferClientCameraFrame(APlayerController* RemotePC, const FRepCamInfo& NewFrame)
{
    
}


bool ASSGameMode::GetBufferedClientCamera(APlayerController* RemotePC, FRepCamInfo& OutFrame)
{
    
    return true;
}

void ASSGameMode::SyncDummyPlayerWithRemotePlayer()
{
    
}



void ASSGameMode::UpdateClientCameraHistory(const FRepCamInfo& ClientCam)
{
    
}

FCameraPredictionData ASSGameMode::PredictClientCameraMovement()
{
    FCameraPredictionData Corrected;
    return Corrected;
}

FCameraPredictionData ASSGameMode::CorrectPredictionWithClientData(
    const FCameraPredictionData& Prediction,
    const FRepCamInfo& ClientData)
{
    FCameraPredictionData Corrected = Prediction;

    return Corrected;
}

void ASSGameMode::ApplyPredictedClientCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
    
}

void ASSGameMode::UpdateSplitScreenLayout()
{

}

// 디버그용 함수
void ASSGameMode::DebugServerCameraPrediction()
{

}