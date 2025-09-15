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

    // �ҽ��� ������� 0�� �÷��̾�� �ڵ� ���� (�ʿ�� ����)
    if (!PC)
    {
        if (UWorld* World = GetWorld())
        {
            PC = UGameplayStatics::GetPlayerController(World, 0);
            SourcePC = PC;
        }
    }

    if (!PC) return;

    // 1) �켱���� 1: ���� PC���� ����/���ŵǴ� PlayerCameraManager
    if (PC->PlayerCameraManager)
    {
        const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
        RepCam.Location = POV.Location;
        RepCam.Rotation = POV.Rotation;
        RepCam.FOV = POV.FOV;
        return;
    }

    // 2) �켱���� 2: ���� PC(�������� Authority���� Local �ƴ�) �� ��Ʈ�ѷ� ȸ�� + Pawn �� ���
    APawn* Pawn = PC->GetPawn();
    if (Pawn)
    {
        // ȸ��: Ŭ�󿡼� ServerUpdateRotation���� �ö�� ��Ʈ�ѷ� ȸ���� ���� �ŷڵ� ����
        const FRotator ControlRot = PC->GetControlRotation();

        // ��ġ/FOV: Pawn�� ī�޶� ������Ʈ�� ������ ���, ������ Pawn ��/�⺻ FOV
        FVector ViewLoc = Pawn->GetPawnViewLocation();
        float   FOV = 90.f;

        if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
        {
            // ��ġ�� ī�޶� ������Ʈ ��ġ�� �� �ڿ�������
            ViewLoc = Cam->GetComponentLocation();
            // ȸ���� ��Ʈ�ѷ� ȸ���� ����(ī�޶� ������Ʈ ȸ���� �������� ���� �� �Ǿ� ���� �� ����)
            FOV = Cam->FieldOfView;
        }

        RepCam.Location = ViewLoc;
        RepCam.Rotation = ControlRot;
        RepCam.FOV = FOV;
        return;
    }

    // 3) Pawn�� ������ ��Ʈ�ѷ������� ���� ä��(������ ����)
    RepCam.Location = GetActorLocation();
    RepCam.Rotation = PC->GetControlRotation();
    RepCam.FOV = 90.f;
}