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
    bAlwaysRelevant = true;   // ��𼭳� �׻� ����
    // NetUpdateFrequency = 30.f;   // �ʿ� �� ����
    SetNetUpdateFrequency(30.f);
    SetReplicateMovement(false); // �츮�� ��ġ/ȸ���� ���� ��ġ�� �� ����, RepCam�� ����

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
        // ������ �ҽ� ���� ����
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
                // ���⼭ SpringArm ��Ÿ�� offset ����
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
        // *** Ŭ���̾�Ʈ: �ڽ��� ���� ��Ʈ�ѷ� POV�� ������ ���� ***
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            if (PC->IsLocalController() && PC->PlayerCameraManager)
            {
                const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
                FVector LocalOffset = PC->GetPawn()->GetActorTransform().InverseTransformPosition(POV.Location);
                FRepCamInfo LocalCam;
                LocalCam.Location = LocalOffset;          // ���� ������ ����
                LocalCam.Rotation = POV.Rotation;         // ȸ���� �״��
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

            // GameMode ���ۿ� ���
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
    // ���⼭ ������ ��ȿ�� ���� ���� (��ġ ���� NaN����, Rotation�� �������� ��)
    return true; // �⺻������ �׻� ���
}