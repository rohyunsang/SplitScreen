// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
// #include "GameFramework/PlayerCameraManager.h"

ASSCameraViewProxy::ASSCameraViewProxy()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    bAlwaysRelevant = true;   // 어디서나 항상 관련
    NetUpdateFrequency = 120.f;   // 필요 시 조정
    SetReplicateMovement(false); // 우리는 위치/회전을 액터 위치로 안 쓰고, RepCam만 복제

    bNetUseOwnerRelevancy = true;  // Owner 기반 관련성 사용

#if WITH_EDITOR
    SetActorLabel(TEXT("ASSCameraViewProxy"));
#endif
}

void ASSCameraViewProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASSCameraViewProxy, RepCam);
    DOREPLIFETIME(ASSCameraViewProxy, RepView);
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
        // *** 서버: Owner가 설정된 경우 (클라이언트 전용 Proxy) ***
        if (APlayerController* OwnerPC = Cast<APlayerController>(GetOwner()))
        {
            // 이 Proxy는 특정 클라이언트 전용이므로 
            // 클라이언트가 RPC로 보낸 데이터만 복제하고, 서버에서 임의로 업데이트하지 않음
            // RepCam은 ServerUpdateClientCamera에서만 업데이트됨

            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Client Proxy for %s - RepCam: %s"),
                *OwnerPC->GetName(), *RepCam.Rotation.ToString());
        }
        // *** 서버: Owner가 없는 경우 (서버 전용 Proxy) ***
        else
        {
            // 서버 전용 Proxy - 리슨 서버(PlayerIndex 0)의 카메라 POV를 복제
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

                UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Server Proxy - RepCam: %s"),
                    *RepCam.Rotation.ToString());
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
                FRepCamInfo LocalCam;
                LocalCam.Location = POV.Location;
                LocalCam.Rotation = POV.Rotation;
                LocalCam.FOV = POV.FOV;

                // RPC → 서버에 전달 (이 클라이언트의 카메라 정보)
                ServerUpdateClientCamera(LocalCam);

                UE_LOG(LogTemp, VeryVerbose, TEXT("SS Client: Sending camera rotation: %s"),
                    *LocalCam.Rotation.ToString());
            }
        }
    }
}

// client -> server 
void ASSCameraViewProxy::ServerUpdateClientCamera_Implementation(const FRepCamInfo& NewCam)
{
    // Owner 체크: 이 Proxy의 Owner 클라이언트만 업데이트 가능
    if (APlayerController* OwningPC = Cast<APlayerController>(GetOwner()))
    {
        // Owner가 RPC를 보낸 클라이언트와 일치하는지 확인
        if (OwningPC == GetNetConnection()->PlayerController)
        {
            RepCam = NewCam;
            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Updated client camera for %s - Rotation: %s"),
                *OwningPC->GetName(), *RepCam.Rotation.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Server: Camera update from wrong client!"));
        }
    }
    else
    {
        // Owner가 없는 서버 전용 Proxy는 클라이언트 RPC를 받으면 안됨
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Received client camera update on server-only proxy!"));
    }
}

bool ASSCameraViewProxy::ServerUpdateClientCamera_Validate(const FRepCamInfo& NewCam)
{
    // 여기서 데이터 유효성 검증 가능 (위치 값이 NaN인지, Rotation이 정상인지 등)
    return true; // 기본적으로 항상 허용
}

void ASSCameraViewProxy::ServerUpdateClientView_Implementation(const FRepPlayerView& NewView)
{
    RepView = NewView; // 서버가 그대로 보관 → 다른 플레이어들에게 복제
}