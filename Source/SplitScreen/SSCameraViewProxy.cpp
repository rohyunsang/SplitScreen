// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
// #include "GameFramework/PlayerCameraManager.h"

ASSCameraViewProxy::ASSCameraViewProxy()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    bAlwaysRelevant = true;   // 어디서나 항상 관련
    NetUpdateFrequency = 120.f;   // 필요 시 조정
    SetReplicateMovement(false); // 우리는 위치/회전을 액터 위치로 안 쓰고, RepCam만 복제

#if WITH_EDITOR
    SetActorLabel(TEXT("ASSCameraViewProxy"));
#endif
}

void ASSCameraViewProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASSCameraViewProxy, RepCam);
}

void ASSCameraViewProxy::SetSourcePC(APlayerController* InPC)
{
    if (!HasAuthority())
    {
        // 서버만 소스 설정 가능
        return;
    }

    SourcePC = InPC;
}

void ASSCameraViewProxy::SetSourceFromPlayerIndex(int32 PlayerIndex /*=0*/)
{
    if (!HasAuthority())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, PlayerIndex);
        SetSourcePC(PC);
    }
}

void ASSCameraViewProxy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 서버에서만 카메라 정보를 채워 복제
    if (!HasAuthority())
    {
        return;
    }

    APlayerController* PC = SourcePC.Get();

    // 소스가 비었으면 0번 플레이어로 자동 설정 (필요시 유지)
    if (!PC)
    {
        if (UWorld* World = GetWorld())
        {
            PC = UGameplayStatics::GetPlayerController(World, 0);
            SourcePC = PC;
        }
    }

    if (!PC) return;

    // 1) 우선순위 1: 로컬 PC에만 존재/갱신되는 PlayerCameraManager
    if (PC->PlayerCameraManager)
    {
        const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
        RepCam.Location = POV.Location;
        RepCam.Rotation = POV.Rotation;
        RepCam.FOV = POV.FOV;
        return;
    }

    // 2) 우선순위 2: 원격 PC(서버에서 Authority지만 Local 아님) → 컨트롤러 회전 + Pawn 뷰 기반
    APawn* Pawn = PC->GetPawn();
    if (Pawn)
    {
        // 회전: 클라에서 ServerUpdateRotation으로 올라온 컨트롤러 회전이 가장 신뢰도 높음
        const FRotator ControlRot = PC->GetControlRotation();

        // 위치/FOV: Pawn의 카메라 컴포넌트가 있으면 사용, 없으면 Pawn 뷰/기본 FOV
        FVector ViewLoc = Pawn->GetPawnViewLocation();
        float   FOV = 90.f;

        if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
        {
            // 위치는 카메라 컴포넌트 위치가 더 자연스러움
            ViewLoc = Cam->GetComponentLocation();
            // 회전은 컨트롤러 회전을 유지(카메라 컴포넌트 회전은 서버에서 갱신 안 되어 있을 수 있음)
            FOV = Cam->FieldOfView;
        }

        RepCam.Location = ViewLoc;
        RepCam.Rotation = ControlRot;
        RepCam.FOV = FOV;
        return;
    }

    // 3) Pawn도 없으면 컨트롤러만으로 대충 채움(최후의 수단)
    RepCam.Location = GetActorLocation();
    RepCam.Rotation = PC->GetControlRotation();
    RepCam.FOV = 90.f;
}