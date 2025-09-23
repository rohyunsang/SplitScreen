// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "SSCameraViewProxy.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;
class ASSCameraViewProxy;
class UCameraComponent;


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

    // 캐시된 프록시 참조
    UPROPERTY()
    TWeakObjectPtr<ASSCameraViewProxy> CachedProxy;

protected:
    // 네트워크 RPC 함수들
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // 입력 설정
    virtual void SetupInputComponent() override;

private:
    // 네트워크 업데이트 관련
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // 클라이언트용 함수들
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // 더미 카메라 적용 함수 (서버 데이터만 사용)
    void ApplyServerCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& ServerCam);

    // 클라이언트 더미 관련 변수들
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;
};
