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
    bAlwaysRelevant = true;   // ��𼭳� �׻� ����
    NetUpdateFrequency = 120.f;   // �ʿ� �� ����
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
        // ���������� ī�޶� ���� ������Ʈ
        UpdateServerCamera();
    }
    else
    {
        // Ŭ���̾�Ʈ���� ������ ī�޶� ���� ����
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
    // Ŭ���̾�Ʈ���� ���� ��Ʈ�ѷ��� ī�޶� ���� ī�޶� ���� ����
    if (UWorld* World = GetWorld())
    {
        // ���� ��Ʈ�ѷ� ã�� (SSPlayerController���� �����ϴ� ClientDummyController)
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(It->Get()))
            {
                // ���� ��Ʈ�ѷ��̰� ���������� ����� ���
                if (SSPC->bIsDummyController && SSPC->GetStateName() == NAME_Spectating)
                {
                    if (SSPC->PlayerCameraManager)
                    {
                        // ī�޶� ��ġ�� ȸ���� ������ ����ȭ
                        SSPC->PlayerCameraManager->SetViewTarget(this);

                        // �Ǵ� ���� ī�޶� ��ġ/ȸ�� ����
                        SSPC->SetControlRotation(RepCam.Rotation);

                        // FOV�� ����ȭ
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

// RepCam�� ����� �� ȣ��Ǵ� �Լ� (���û���)
void ASSCameraViewProxy::OnRep_RepCam()
{
    // ������ ī�޶� ������ ������Ʈ�� �� ��� ����
    if (!HasAuthority())
    {
        ApplyReplicatedCamera();
    }
}