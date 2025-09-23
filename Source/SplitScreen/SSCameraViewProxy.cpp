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

    // ���������� ī�޶� ������ ä�� ����
    if (!HasAuthority())
    {
        return;
    }

    APlayerController* PC = SourcePC.Get();

    // �ҽ��� ������� 0�� �÷��̾�(�������� ����)�� �ڵ� ���� �õ�
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
        // ������ ���� ���� �ִ� ȭ�� ����(FMinimalViewInfo)�� �����´�.
        const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();

        RepCam.Location = POV.Location;
        RepCam.Rotation = POV.Rotation;
        RepCam.FOV = POV.FOV;

        // �ʿ� �� ��Ʈ ����(����� ȣ���� Ʈ���� ���� �� �󵵴� ��Ȳ ���� ����)
        // NetUpdateFrequency�� ����ϸ� �����ص� ��
        // ForceNetUpdate();
    }
}