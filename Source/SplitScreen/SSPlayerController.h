// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "SSCameraViewProxy.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;

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

    // 스펙테이터 모드 설정
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetupSpectatorMode(APlayerController* TargetPC);

    // 스펙테이터 타겟 설정
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetSpectatorTarget(APawn* TargetPawn);

protected:
    // 네트워크 RPC 함수들
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // 서버에서 클라이언트에게 스펙테이터 타겟 설정 지시
    UFUNCTION(Client, Reliable)
    void ClientSetSpectatorTarget(APawn* TargetPawn);

    // 입력 설정
    virtual void SetupInputComponent() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_RepCam();

    UPROPERTY(ReplicatedUsing = OnRep_RepCam, BlueprintReadOnly)
    FRepCamInfo RepCam;

private:
    // 네트워크 업데이트 관련
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // 클라이언트용 함수들
    void SetupClientSplitScreen();
    void CreateClientDummyController();
    void SetupSpectatorForDummyController();

    // 클라이언트 더미 관련 변수들
    UPROPERTY()
    APlayerController* ClientDummyController;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;

    // 스펙테이터 관련 변수들
    UPROPERTY()
    APawn* CurrentSpectatorTarget;

    UPROPERTY(EditAnywhere, Category = "Spectator")
    float SpectatorBlendTime = 0.5f; // 타겟 변경시 블렌드 시간

    // 타겟 플레이어 찾기
    APawn* FindRemotePlayerPawn();
    APlayerController* FindRemotePlayerController();


};