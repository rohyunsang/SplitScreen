// Fill out your copyright notice in the Description page of Project Settings.


#include "SSDummySpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

ASSDummySpectatorPawn::ASSDummySpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // ī�޶� ������Ʈ ����
    DummyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DummyCamera"));
    RootComponent = DummyCamera;

    // �⺻ ���������� ����
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = true;
    bUseControllerRotationRoll = false;
}

void ASSDummySpectatorPawn::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("SS Dummy Spectator Pawn Created"));
}

void ASSDummySpectatorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bHasTarget)
    {
        // �ε巴�� Ÿ�� ��ġ�� �̵�
        FVector CurrentLocation = GetActorLocation();
        FRotator CurrentRotation = GetActorRotation();

        FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, SmoothingSpeed);
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, SmoothingSpeed);

        SetActorLocationAndRotation(NewLocation, NewRotation);
    }
}

void ASSDummySpectatorPawn::SyncWithRemotePlayer(FVector Location, FRotator Rotation)
{
    TargetLocation = Location;
    TargetRotation = Rotation;
    bHasTarget = true;
}

