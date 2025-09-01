// PartnerSpectatorPawn.cpp

#include "Pawn/PartnerSpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/SphereComponent.h" 

APartnerSpectatorPawn::APartnerSpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // �浹 ��Ȱ��
    if (GetCollisionComponent())
    {
        GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCollisionComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    // ��������
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 300.f;                // �⺻ 3rd-person
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 15.f;

    // ī�޶�
    PartnerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PartnerCamera"));
    PartnerCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    PartnerCamera->bUsePawnControlRotation = false;
    PartnerCamera->SetFieldOfView(90.f);

    // ��Ʈ��ũ/����
    bReplicates = false;
    SetReplicateMovement(false);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

void APartnerSpectatorPawn::BeginPlay()
{
    Super::BeginPlay();

    SetActorHiddenInGame(true); // ��ü�� ����

    TArray<UMeshComponent*> MeshComponents;
    GetComponents<UMeshComponent>(MeshComponents);
    for (UMeshComponent* M : MeshComponents)
    {
        if (M) M->SetVisibility(false);
    }
}

void APartnerSpectatorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bUseSmoothMovement)
    {
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaTime, InterpolationSpeed));
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, InterpolationSpeed));

        if (CameraBoom && TargetArmLength >= 0.f)
        {
            CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, InterpolationSpeed);
        }
    }
}

void APartnerSpectatorPawn::UpdateFromPartner(FVector Location, FRotator Rotation, float ArmLength)
{
    if (bUseSmoothMovement)
    {
        TargetLocation = Location;
        TargetRotation = Rotation;
        TargetArmLength = ArmLength;
    }
    else
    {
        SetActorLocation(Location);
        SetActorRotation(Rotation);
        if (CameraBoom && ArmLength >= 0.f) CameraBoom->TargetArmLength = ArmLength;
    }
}

void APartnerSpectatorPawn::UpdateFromPartnerSmooth(FVector Location, FRotator Rotation, float ArmLength)
{
    TargetLocation = Location;
    TargetRotation = Rotation;
    TargetArmLength = ArmLength;
}

void APartnerSpectatorPawn::SetCameraFOV(float NewFOV)
{
    if (PartnerCamera)
    {
        PartnerCamera->SetFieldOfView(FMath::Clamp(NewFOV, 60.f, 120.f));
    }
}

void APartnerSpectatorPawn::SetRenderQuality(bool bHighQuality)
{
    if (!PartnerCamera) return;

    if (bHighQuality)
    {
        PartnerCamera->PostProcessSettings.bOverride_MotionBlurAmount = false;
        PartnerCamera->PostProcessSettings.bOverride_AmbientOcclusionQuality = false;
    }
    else
    {
        PartnerCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
        PartnerCamera->PostProcessSettings.MotionBlurAmount = 0.f;
        PartnerCamera->PostProcessSettings.bOverride_AmbientOcclusionQuality = true;
        PartnerCamera->PostProcessSettings.AmbientOcclusionQuality = 0.f;
    }
}

void APartnerSpectatorPawn::SetViewType(bool bThirdPerson, float ArmLength)
{
    if (!CameraBoom) return;

    if (bThirdPerson)
    {
        CameraBoom->TargetArmLength = ArmLength;
        CameraBoom->bUsePawnControlRotation = true;
    }
    else
    {
        CameraBoom->TargetArmLength = 0.f;
        CameraBoom->bUsePawnControlRotation = false;
    }
}
