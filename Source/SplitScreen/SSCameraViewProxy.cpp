// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "SSGameMode.h"
// #include "GameFramework/PlayerCameraManager.h"

ASSCameraViewProxy::ASSCameraViewProxy()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    bAlwaysRelevant = true;   // 어디서나 항상 관련
    // NetUpdateFrequency = 30.f;   // 필요 시 조정
    SetNetUpdateFrequency(30.f);
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
        if (APlayerController* OwnerPC = Cast<APlayerController>(GetOwner()))
        {
            if (APawn* P = OwnerPC->GetPawn())
            {
                // 여기서 SpringArm 스타일 offset 적용
                FVector CameraLoc = P->GetActorLocation() + FVector(-200, 0, 80);
                FRotator CameraRot = OwnerPC->GetControlRotation();

                RepCam.Location = CameraLoc;
                RepCam.Rotation = CameraRot;
                RepCam.FOV = 90.f;
            }
        }
    }
    else
    {
        // *** 클라이언트: 자신의 로컬 컨트롤러 POV를 서버로 전송 ***
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            if (PC->IsLocalController() && PC->PlayerCameraManager)
            {
                const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
                FVector LocalOffset = PC->GetPawn()->GetActorTransform().InverseTransformPosition(POV.Location);
                FRepCamInfo LocalCam;
                LocalCam.Location = LocalOffset;          // 로컬 오프셋 저장
                LocalCam.Rotation = POV.Rotation;         // 회전은 그대로
                LocalCam.FOV = POV.FOV;

                ServerUpdateClientCamera(LocalCam);
            }
        }
    }
}

// client -> server 
void ASSCameraViewProxy::ServerUpdateClientCamera_Implementation(const FRepCamInfo& NewCam)
{
    if (APlayerController* OwningPC = Cast<APlayerController>(GetOwner()))
    {
        if (OwningPC == GetNetConnection()->PlayerController)
        {
            RepCam = NewCam;

            // GameMode 버퍼에 기록
            if (UWorld* World = GetWorld())
            {
                if (ASSGameMode* GM = Cast<ASSGameMode>(World->GetAuthGameMode()))
                {
                    GM->BufferClientCameraFrame(OwningPC, RepCam);
                }
            }
        }
    }
}

bool ASSCameraViewProxy::ServerUpdateClientCamera_Validate(const FRepCamInfo& NewCam)
{
    // 여기서 데이터 유효성 검증 가능 (위치 값이 NaN인지, Rotation이 정상인지 등)
    return true; // 기본적으로 항상 허용
}