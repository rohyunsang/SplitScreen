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

    // ���� ��Ʈ�ѷ� �÷���
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bIsDummyController = false;

    // ���� ��Ʈ�ѷ��� ����
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetAsDummyController(bool bDummy = true);

    // ĳ�õ� ���Ͻ� ����
    UPROPERTY()
    TWeakObjectPtr<ASSCameraViewProxy> CachedProxy;

protected:
    // ��Ʈ��ũ RPC �Լ���
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // �Է� ����
    virtual void SetupInputComponent() override;

private:
    // ��Ʈ��ũ ������Ʈ ����
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // Ŭ���̾�Ʈ�� �Լ���
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // ���� ī�޶� ���� �Լ� (���� �����͸� ���)
    void ApplyServerCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& ServerCam);

    // Ŭ���̾�Ʈ ���� ���� ������
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;
};
