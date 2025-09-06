// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;
class ASSCameraViewProxy;
class UCameraComponent;

// ī�޶� ������ ���� ������ ����ü
USTRUCT()
struct FCameraPredictionData
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    float FOV = 90.0f;

    UPROPERTY()
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY()
    FVector AngularVelocity = FVector::ZeroVector;

    UPROPERTY()
    float Timestamp = 0.0f;

    FCameraPredictionData()
    {
        Location = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        FOV = 90.0f;
        Velocity = FVector::ZeroVector;
        AngularVelocity = FVector::ZeroVector;
        Timestamp = 0.0f;
    }
};

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

    // ī�޶� ȸ�� ���� 
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float MinimumMovementThreshold = 5.0f; // �ּ� ������ �Ӱ谪 (cm)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float MinimumRotationThreshold = 2.0f; // �ּ� ȸ�� �Ӱ谪 (��)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float StationaryPredictionReduction = 0.3f; // ���� ������ �� ���� ���� ����

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float MaxAngularVelocityMagnitude = 180.0f; // �ִ� ���ӵ� ���� (��/��)

private:
    // ��Ʈ��ũ ������Ʈ ����
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // Ŭ���̾�Ʈ�� �Լ���
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // Ŭ���̾�Ʈ ���� ���� ������
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;

    // === ī�޶� ���� �ý��� ===

    // �����κ��� ���� ī�޶� ������ �����丮
    UPROPERTY()
    TArray<FCameraPredictionData> CameraHistory;

    // ���� ������ ī�޶� ����
    UPROPERTY()
    FCameraPredictionData PredictedCamera;

    // ���������� ���� ���� ������
    UPROPERTY()
    FCameraPredictionData LastServerCamera;

    // ���� ��������
    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float MaxPredictionTime = 0.2f; // �ִ� ���� �ð� (200ms)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float CorrectionSpeed = 10.0f; // ���� �����ͷ� �����ϴ� �ӵ�

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    int32 MaxHistorySize = 10; // ������ �����丮 ����

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionLocationThreshold = 100.0f; // ��� ������ ��ġ ���� �Ӱ谪 (cm)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionRotationThreshold = 10.0f; // ��� ������ ȸ�� ���� �Ӱ谪 (��)

    // ī�޶� ���� ���� �Լ���
    void UpdateCameraHistory(const struct FRepCamInfo& ServerCam);
    FCameraPredictionData PredictCameraMovement();
    FCameraPredictionData CorrectPredictionWithServerData(const FCameraPredictionData& Prediction, const struct FRepCamInfo& ServerData);
    void ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData);

    // ����׿� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugCameraPrediction();



    // ���� ������ ���� ����
    bool bIsMoving = false;
    bool bIsRotating = false;
    float StationaryTime = 0.0f;
};
