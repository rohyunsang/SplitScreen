// Fill out your copyright notice in the Description page of Project Settings.


#include "SSDummySpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

ASSDummySpectatorPawn::ASSDummySpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // 카메라 컴포넌트 생성
    DummyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DummyCamera"));
    RootComponent = DummyCamera;

    // 기본 스펙테이터 설정
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
        // 부드럽게 타겟 위치로 이동
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

