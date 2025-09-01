// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "PartnerSpectatorPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;


/**
 * 
 */
UCLASS()
class SPLITSCREEN_API APartnerSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
    APartnerSpectatorPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 파트너로부터 받은 데이터 반영
    void UpdateFromPartner(FVector Location, FRotator Rotation, float ArmLength);
    void UpdateFromPartnerSmooth(FVector Location, FRotator Rotation, float ArmLength);
    void SetCameraFOV(float NewFOV);
    void SetRenderQuality(bool bHighQuality);
    void SetViewType(bool bThirdPerson, float ArmLength);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PartnerView")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PartnerView")
    TObjectPtr<UCameraComponent> PartnerCamera;

    // 보간 설정
    UPROPERTY(EditAnywhere, Category = "PartnerView")
    float InterpolationSpeed = 10.f;

    UPROPERTY(EditAnywhere, Category = "PartnerView")
    bool bUseSmoothMovement = true;

private:
    FVector TargetLocation = FVector::ZeroVector;
    FRotator TargetRotation = FRotator::ZeroRotator;
    float TargetArmLength = 300.f;
	
};
