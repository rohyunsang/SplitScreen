// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
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
    Super::Tick(DeltaSeconds);

    // 서버에서만 카메라 정보를 채워 복제
    if (!HasAuthority())
    {
        return;
    }

    APlayerController* PC = SourcePC.Get();

    // 소스가 비었으면 0번 플레이어(리슨서버 로컬)로 자동 설정 시도
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
        // 서버가 실제 보고 있는 화면 시점(FMinimalViewInfo)을 가져온다.
        const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();

        RepCam.Location = POV.Location;
        RepCam.Rotation = POV.Rotation;
        RepCam.FOV = POV.FOV;

        // SpringArm 길이 추가 캡처
        if (APawn* SourcePawn = PC->GetPawn())
        {
            if (USpringArmComponent* SpringArm = SourcePawn->FindComponentByClass<USpringArmComponent>())
            {
                RepCam.SpringArmLength = SpringArm->TargetArmLength;
            }
        }

        // 필요 시 네트 갱신(빈번한 호출은 트래픽 증가 → 빈도는 상황 맞춰 조절)
        // NetUpdateFrequency로 충분하면 생략해도 됨
        // ForceNetUpdate();
    }
}