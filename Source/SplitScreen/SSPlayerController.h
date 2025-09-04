// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SSPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

protected:
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

private:
    float LocationUpdateInterval = 0.1f; // 10fps
    float TimeSinceLastUpdate = 0.0f;
};
