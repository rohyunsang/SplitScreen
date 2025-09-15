// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "SSPlayerController.h"
// #include "GameFramework/PlayerCameraManager.h"

ASSCameraViewProxy::ASSCameraViewProxy()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    bAlwaysRelevant = true;   // 어디서나 항상 관련
    SetNetUpdateFrequency(120.f);   // 필요 시 조정
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
    if (!HasAuthority()) return;

    APlayerController* PC = SourcePC.Get();
    if (!PC) return;

    if (APawn* Pawn = PC->GetPawn())
    {
        FRepCamInfo NewCamInfo;
        NewCamInfo.Location = Pawn->GetPawnViewLocation();
        NewCamInfo.FOV = 90.f;
        NewCamInfo.Timestamp = GetWorld()->GetTimeSeconds();

        // 카메라 컴포넌트가 있으면 더 정확한 정보 사용
        if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
        {
            NewCamInfo.Location = Cam->GetComponentLocation();
            NewCamInfo.FOV = Cam->FieldOfView;
        }

        // 회전 정보 수집 - 로컬/원격 구분
        if (PC->IsLocalController())
        {
            // 로컬(서버) 플레이어는 직접 ControlRotation 사용
            NewCamInfo.Rotation = PC->GetControlRotation();
        }
        else
        {
            // 원격 플레이어는 복제된 회전값 사용
            if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(PC))
            {
                NewCamInfo.Rotation = SSPC->GetReplicatedControlRotation();  //  getter 사용

                // 복제된 회전값이 아직 없으면 Pawn의 BaseAimRotation 사용 (폴백)
                if (NewCamInfo.Rotation.IsNearlyZero())
                {
                    NewCamInfo.Rotation = Pawn->GetBaseAimRotation();
                    UE_LOG(LogTemp, Warning, TEXT("Using BaseAimRotation as fallback"));
                }
            }
            else
            {
                // SSPlayerController가 아닌 경우 기본값 사용
                NewCamInfo.Rotation = PC->GetControlRotation();
            }
        }

        // 변경되었을 때만 업데이트
        if (!RepCam.Rotation.Equals(NewCamInfo.Rotation, 1.0f) ||
            !RepCam.Location.Equals(NewCamInfo.Location, 1.0f))
        {
            RepCam = NewCamInfo;

            // 디버그 로그
            UE_LOG(LogTemp, VeryVerbose, TEXT("CamProxy updated - PC: %s, Local: %s, Rotation: %s"),
                *PC->GetName(),
                PC->IsLocalController() ? TEXT("Yes") : TEXT("No"),
                *NewCamInfo.Rotation.ToString());
        }
    }
}