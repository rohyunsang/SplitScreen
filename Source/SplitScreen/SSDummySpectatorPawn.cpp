// Fill out your copyright notice in the Description page of Project Settings.


#include "SSDummySpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"

ASSDummySpectatorPawn::ASSDummySpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    RootComponent = CameraBoom;
    CameraBoom->TargetArmLength = 400.f;          // Ÿ�ٰ� ī�޶� �Ÿ�
    CameraBoom->bUsePawnControlRotation = true;   // ��Ʈ�ѷ� ȸ�� = �� ȸ��
    CameraBoom->bDoCollisionTest = false;

    // ���̴� ���� ���� ���� �� �� (Ŭ�󿡼� �̹� ���װ� �ݿ��� ���� �並 ����)
    CameraBoom->bEnableCameraLag = false;
    CameraBoom->bEnableCameraRotationLag = false;

    DummyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    DummyCamera->SetupAttachment(CameraBoom);
    DummyCamera->bUsePawnControlRotation = false;

    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    // ���� ���� �Ⱥ��̰� ����
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void ASSDummySpectatorPawn::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("[NetMode : %d] SS Dummy Spectator Pawn Created - Camera Sync Mode: %s"),
        GetWorld()->GetNetMode(),
        bSyncDirectlyToCamera ? TEXT("Direct Camera Sync") : TEXT("Position + Offset"));
}

void ASSDummySpectatorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bSyncDirectlyToCamera && TargetCamera)
    {
        //  ī�޶� ���� ����ȭ ���
        FVector TargetCameraLocation = TargetCamera->GetComponentLocation();
        FRotator TargetCameraRotation = TargetCamera->GetComponentRotation();

        FVector CurrentLocation = GetActorLocation();
        FRotator CurrentRotation = GetActorRotation();

        FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetCameraLocation, DeltaTime, SmoothingSpeed);
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetCameraRotation, DeltaTime, SmoothingSpeed);

        SetActorLocationAndRotation(NewLocation, NewRotation);
    }
    else if (bHasTarget)
    {
        // ���� ��ġ + ������ ���
        FVector CurrentLocation = GetActorLocation();
        FRotator CurrentRotation = GetActorRotation();

        FVector DesiredLocation;
        FRotator DesiredRotation;

        if (bUseOffsetFromTarget)
        {
            DesiredLocation = CalculateOffsetPosition(TargetLocation, TargetRotation);

            if (bLookAtTarget)
            {
                DesiredRotation = CalculateLookAtRotation(DesiredLocation, TargetLocation);
            }
            else
            {
                DesiredRotation = TargetRotation;
            }
        }
        else
        {
            DesiredLocation = TargetLocation;
            DesiredRotation = TargetRotation;
        }

        float DistanceToTarget = FVector::Dist(DesiredLocation, TargetLocation);
        if (DistanceToTarget < MinDistanceFromTarget)
        {
            FVector DirectionFromTarget = (DesiredLocation - TargetLocation).GetSafeNormal();
            DesiredLocation = TargetLocation + (DirectionFromTarget * MinDistanceFromTarget);
        }

        FVector NewLocation = FMath::VInterpTo(CurrentLocation, DesiredLocation, DeltaTime, SmoothingSpeed);
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, SmoothingSpeed);

        SetActorLocationAndRotation(NewLocation, NewRotation);
    }

    //  Ÿ���� ��������� üũ�ϰ� �ڵ����� ��Ž��
    if (!TargetCamera && TargetPawn)
    {
        UpdateTargetCamera();
    }
}

void ASSDummySpectatorPawn::SyncWithRemotePlayer(FVector Location, FRotator Rotation)
{
    TargetLocation = Location;
    TargetRotation = Rotation;
    bHasTarget = true;

    if (!bSyncDirectlyToCamera)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy syncing to position: %s"), *Location.ToString());
    }
}

//  ī�޶� ���� ����ȭ �Լ�
void ASSDummySpectatorPawn::SyncWithRemoteCamera(UCameraComponent* RemoteCamera)
{
    if (!RemoteCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy: Remote camera is null"));
        return;
    }

    TargetCamera = RemoteCamera;
    bSyncDirectlyToCamera = true;

    UE_LOG(LogTemp, Warning, TEXT("SS Dummy: Now syncing directly to camera: %s"),
        *RemoteCamera->GetName());
}

//  Ÿ�� �� ���� �Լ�
void ASSDummySpectatorPawn::SetTargetPawn(APawn* NewTargetPawn)
{
    if (!NewTargetPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy: Target pawn is null"));
        TargetPawn = nullptr;
        TargetCamera = nullptr;
        return;
    }

    TargetPawn = NewTargetPawn;
    UpdateTargetCamera();

    UE_LOG(LogTemp, Warning, TEXT("SS Dummy: Target pawn set to: %s"),
        *TargetPawn->GetName());
}

//  Ÿ�� ī�޶� ������Ʈ �Լ�
void ASSDummySpectatorPawn::UpdateTargetCamera()
{
    if (!TargetPawn)
        return;

    TargetCamera = FindCameraInPawn(TargetPawn);

    if (TargetCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy: Found camera in target pawn: %s"),
            *TargetCamera->GetName());

        if (bSyncDirectlyToCamera)
        {
            bHasTarget = false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy: No camera found in target pawn, using position sync"));
        bSyncDirectlyToCamera = false;
    }
}

//  ������ ī�޶� ã�� �Լ�
UCameraComponent* ASSDummySpectatorPawn::FindCameraInPawn(APawn* Pawn)
{
    if (!Pawn)
        return nullptr;

    // 1. ���� CameraComponent ã��
    UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
    if (Camera)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy: Found camera directly: %s"), *Camera->GetName());
        return Camera;
    }

    // 2. ��������� ��� �ڽ� ������Ʈ �˻�
    TArray<UCameraComponent*> CameraComponents;
    Pawn->GetComponents<UCameraComponent>(CameraComponents);

    if (CameraComponents.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy: Found camera recursively: %s"), *CameraComponents[0]->GetName());
        return CameraComponents[0];
    }

    UE_LOG(LogTemp, Warning, TEXT("SS Dummy: No camera found in pawn: %s"), *Pawn->GetName());
    return nullptr;
}

FVector ASSDummySpectatorPawn::CalculateOffsetPosition(const FVector& TargetPos, const FRotator& TargetRot)
{
    FVector RotatedOffset = TargetRot.RotateVector(CameraOffset);
    return TargetPos + RotatedOffset;
}

FRotator ASSDummySpectatorPawn::CalculateLookAtRotation(const FVector& FromPos, const FVector& ToPos)
{
    FVector Direction = (ToPos - FromPos).GetSafeNormal();
    return FRotationMatrix::MakeFromX(Direction).Rotator();
}