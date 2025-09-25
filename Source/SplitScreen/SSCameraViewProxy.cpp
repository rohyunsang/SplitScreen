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
    bAlwaysRelevant = true;   // ��𼭳� �׻� ����
    NetUpdateFrequency = 120.f;   // �ʿ� �� ����
    SetReplicateMovement(false); // �츮�� ��ġ/ȸ���� ���� ��ġ�� �� ����, RepCam�� ����

    bNetUseOwnerRelevancy = true;  // Owner ��� ���ü� ���

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
        // *** ����: Owner�� ������ ��� (Ŭ���̾�Ʈ ���� Proxy) ***
        if (APlayerController* OwnerPC = Cast<APlayerController>(GetOwner()))
        {
            // �� Proxy�� Ư�� Ŭ���̾�Ʈ �����̹Ƿ� 
            // Ŭ���̾�Ʈ�� RPC�� ���� �����͸� �����ϰ�, �������� ���Ƿ� ������Ʈ���� ����
            // RepCam�� ServerUpdateClientCamera������ ������Ʈ��

            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Client Proxy for %s - RepCam: %s"),
                *OwnerPC->GetName(), *RepCam.Rotation.ToString());
        }
        // *** ����: Owner�� ���� ��� (���� ���� Proxy) ***
        else
        {
            // ���� ���� Proxy - ���� ����(PlayerIndex 0)�� ī�޶� POV�� ����
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
        // *** Ŭ���̾�Ʈ: �ڽ��� ���� ��Ʈ�ѷ� POV�� ������ ���� ***
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            if (PC->IsLocalController() && PC->PlayerCameraManager)
            {
                const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
                FRepCamInfo LocalCam;
                LocalCam.Location = POV.Location;
                LocalCam.Rotation = POV.Rotation;
                LocalCam.FOV = POV.FOV;

                // RPC �� ������ ���� (�� Ŭ���̾�Ʈ�� ī�޶� ����)
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
    // Owner üũ: �� Proxy�� Owner Ŭ���̾�Ʈ�� ������Ʈ ����
    if (APlayerController* OwningPC = Cast<APlayerController>(GetOwner()))
    {
        // Owner�� RPC�� ���� Ŭ���̾�Ʈ�� ��ġ�ϴ��� Ȯ��
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
        // Owner�� ���� ���� ���� Proxy�� Ŭ���̾�Ʈ RPC�� ������ �ȵ�
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Received client camera update on server-only proxy!"));
    }
}

bool ASSCameraViewProxy::ServerUpdateClientCamera_Validate(const FRepCamInfo& NewCam)
{
    // ���⼭ ������ ��ȿ�� ���� ���� (��ġ ���� NaN����, Rotation�� �������� ��)
    return true; // �⺻������ �׻� ���
}

void ASSCameraViewProxy::ServerUpdateClientView_Implementation(const FRepPlayerView& NewView)
{
    RepView = NewView; // ������ �״�� ���� �� �ٸ� �÷��̾�鿡�� ����
}