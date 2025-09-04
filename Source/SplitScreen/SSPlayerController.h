// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;

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

    // 더미 컨트롤러 플래그
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bIsDummyController = false;

    // 더미 컨트롤러로 설정
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetAsDummyController(bool bDummy = true);

protected:
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // 입력 설정
    virtual void SetupInputComponent() override;

private:
    float LocationUpdateInterval = 0.1f; // 10fps
    float TimeSinceLastUpdate = 0.0f;

    // 클라이언트용 함수들
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn); // 이 줄이 빠져있었음

    // 클라이언트 더미 관련 변수들
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;
};
