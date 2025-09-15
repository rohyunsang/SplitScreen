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
    bAlwaysRelevant = true;   // ��𼭳� �׻� ����
    SetNetUpdateFrequency(120.f);   // �ʿ� �� ����
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
    if (!HasAuthority()) return;

    APlayerController* PC = SourcePC.Get();
    if (!PC) return;

    if (APawn* Pawn = PC->GetPawn())
    {
        FRepCamInfo NewCamInfo;
        NewCamInfo.Location = Pawn->GetPawnViewLocation();
        NewCamInfo.FOV = 90.f;
        NewCamInfo.Timestamp = GetWorld()->GetTimeSeconds();

        // ī�޶� ������Ʈ�� ������ �� ��Ȯ�� ���� ���
        if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
        {
            NewCamInfo.Location = Cam->GetComponentLocation();
            NewCamInfo.FOV = Cam->FieldOfView;
        }

        // ȸ�� ���� ���� - ����/���� ����
        if (PC->IsLocalController())
        {
            // ����(����) �÷��̾�� ���� ControlRotation ���
            NewCamInfo.Rotation = PC->GetControlRotation();
        }
        else
        {
            // ���� �÷��̾�� ������ ȸ���� ���
            if (ASSPlayerController* SSPC = Cast<ASSPlayerController>(PC))
            {
                NewCamInfo.Rotation = SSPC->GetReplicatedControlRotation();  //  getter ���

                // ������ ȸ������ ���� ������ Pawn�� BaseAimRotation ��� (����)
                if (NewCamInfo.Rotation.IsNearlyZero())
                {
                    NewCamInfo.Rotation = Pawn->GetBaseAimRotation();
                    UE_LOG(LogTemp, Warning, TEXT("Using BaseAimRotation as fallback"));
                }
            }
            else
            {
                // SSPlayerController�� �ƴ� ��� �⺻�� ���
                NewCamInfo.Rotation = PC->GetControlRotation();
            }
        }

        // ����Ǿ��� ���� ������Ʈ
        if (!RepCam.Rotation.Equals(NewCamInfo.Rotation, 1.0f) ||
            !RepCam.Location.Equals(NewCamInfo.Location, 1.0f))
        {
            RepCam = NewCamInfo;

            // ����� �α�
            UE_LOG(LogTemp, VeryVerbose, TEXT("CamProxy updated - PC: %s, Local: %s, Rotation: %s"),
                *PC->GetName(),
                PC->IsLocalController() ? TEXT("Yes") : TEXT("No"),
                *NewCamInfo.Rotation.ToString());
        }
    }
}