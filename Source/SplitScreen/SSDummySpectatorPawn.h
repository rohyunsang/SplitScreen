// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "SSDummySpectatorPawn.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSDummySpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()
	
public:
    ASSDummySpectatorPawn();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* DummyCamera;

    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SyncWithRemotePlayer(FVector Location, FRotator Rotation);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync")
    float SmoothingSpeed = 5.0f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    FVector TargetLocation;
    FRotator TargetRotation;
    bool bHasTarget = false;
};
