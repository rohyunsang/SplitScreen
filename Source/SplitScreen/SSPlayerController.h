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

    // ���� ��Ʈ�ѷ� �÷���
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bIsDummyController = false;

    // ���� ��Ʈ�ѷ��� ����
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetAsDummyController(bool bDummy = true);

protected:
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // �Է� ����
    virtual void SetupInputComponent() override;

private:
    float LocationUpdateInterval = 0.1f; // 10fps
    float TimeSinceLastUpdate = 0.0f;

    // Ŭ���̾�Ʈ�� �Լ���
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn); // �� ���� �����־���

    // Ŭ���̾�Ʈ ���� ���� ������
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;
};
