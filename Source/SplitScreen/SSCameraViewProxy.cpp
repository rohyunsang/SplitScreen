// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "SSPlayerController.h"
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

    if (HasAuthority())
    {
        // 서버에서만 카메라 정보 업데이트
        UpdateServerCamera();
    }
    else
    {
        // 클라이언트에서 복제된 카메라 정보 적용
        ApplyReplicatedCamera();
    }
}

void ASSCameraViewProxy::UpdateServerCamera()
{
    APlayerController* PC = SourcePC.Get();

    if (!PC)
    {
        if (UWorld* World = GetWorld())
        {
            PC = UGameplayStatics::GetPlayerController(World, 0);
            SourcePC = PC;
        }
    }

    if (PC && PC->PlayerCameraManager)
    {
        const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
        RepCam.Location = POV.Location;
        RepCam.Rotation = POV.Rotation;
        RepCam.FOV = POV.FOV;
    }
}

void ASSCameraViewProxy::ApplyReplicatedCamera()
{
    // 클라이언트에서 더미 컨트롤러의 카메라에 서버 카메라 정보 적용
    if (UWorld* World = GetWorld())
    {
        // 더미 컨트롤러 찾기 (SSPlayerController에서 관리하는 ClientDummyController)
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(It->Get()))
            {
                // 더미 컨트롤러이고 스펙테이터 모드인 경우
                if (SSPC->bIsDummyController && SSPC->GetStateName() == NAME_Spectating)
                {
                    if (SSPC->PlayerCameraManager)
                    {
                        // 카메라 위치와 회전을 서버와 동기화
                        SSPC->PlayerCameraManager->SetViewTarget(this);

                        // 또는 직접 카메라 위치/회전 설정
                        SSPC->SetControlRotation(RepCam.Rotation);

                        // FOV도 동기화
                        if (SSPC->PlayerCameraManager)
                        {
                            SSPC->PlayerCameraManager->SetFOV(RepCam.FOV);
                        }
                    }
                    break;
                }
            }
        }
    }
}

// RepCam이 변경될 때 호출되는 함수 (선택사항)
void ASSCameraViewProxy::OnRep_RepCam()
{
    // 복제된 카메라 정보가 업데이트될 때 즉시 적용
    if (!HasAuthority())
    {
        ApplyReplicatedCamera();
    }
}