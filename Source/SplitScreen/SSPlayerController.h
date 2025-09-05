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

    // ���� ��Ʈ�ѷ� �÷���
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bIsDummyController = false;

    // ���� ��Ʈ�ѷ��� ����
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetAsDummyController(bool bDummy = true);

    // ���������� ��� ����
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetupSpectatorMode(APlayerController* TargetPC);

    // ���������� Ÿ�� ����
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetSpectatorTarget(APawn* TargetPawn);

protected:
    // ��Ʈ��ũ RPC �Լ���
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // �������� Ŭ���̾�Ʈ���� ���������� Ÿ�� ���� ����
    UFUNCTION(Client, Reliable)
    void ClientSetSpectatorTarget(APawn* TargetPawn);

    // �Է� ����
    virtual void SetupInputComponent() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_RepCam();

    UPROPERTY(ReplicatedUsing = OnRep_RepCam, BlueprintReadOnly)
    FRepCamInfo RepCam;

private:
    // ��Ʈ��ũ ������Ʈ ����
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // Ŭ���̾�Ʈ�� �Լ���
    void SetupClientSplitScreen();
    void CreateClientDummyController();
    void SetupSpectatorForDummyController();

    // Ŭ���̾�Ʈ ���� ���� ������
    UPROPERTY()
    APlayerController* ClientDummyController;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;

    // ���������� ���� ������
    UPROPERTY()
    APawn* CurrentSpectatorTarget;

    UPROPERTY(EditAnywhere, Category = "Spectator")
    float SpectatorBlendTime = 0.5f; // Ÿ�� ����� ���� �ð�

    // Ÿ�� �÷��̾� ã��
    APawn* FindRemotePlayerPawn();
    APlayerController* FindRemotePlayerController();


};