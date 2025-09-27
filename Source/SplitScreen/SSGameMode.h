// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "SSCameraViewProxy.h"
#include "SSGameMode.generated.h"

struct FCameraPredictionData;

USTRUCT()
struct FBufferedCamFrame
{
    GENERATED_BODY()
    FRepCamInfo Data;
    float Timestamp;
};

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSGameMode : public AGameModeBase        
{
	GENERATED_BODY()
	
public:
    ASSGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // Ŭ�󿡼� ī�޶� ������ �߰�
    void BufferClientCameraFrame(APlayerController* RemotePC, const FRepCamInfo& NewFrame);

    // ���� ������ ������ ��������
    bool GetBufferedClientCamera(APlayerController* RemotePC, FRepCamInfo& OutFrame);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<APawn> DummySpectatorPawnClass;

    UPROPERTY() // GC ��ȣ
        class ASSCameraViewProxy* ServerCamProxy = nullptr;

    UPROPERTY()
    TMap<APlayerController*, ASSCameraViewProxy*> ClientCamProxies;

    UPROPERTY()
    TMap<APlayerController*, ASSDummySpectatorPawn*> ClientSpectatorPawns;

protected:
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetupOnlineSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void UpdateSplitScreenLayout();

protected:
    // ����
    TMap<APlayerController*, TArray<FBufferedCamFrame>> ClientCamBuffers;

    // ms ������
    float InterpDelay = 0.05f;

    // Dummy
    class ASSDummySpectatorPawn* DummySpectatorPawn = nullptr;
    class ASSPlayerController* DummyPlayerController = nullptr;

    // Connected Players
    TArray<APlayerController*> ConnectedPlayers;

private:



    void CreateDummyLocalPlayer();
    void SyncDummyPlayerWithRemotePlayer();

    FTimerHandle SyncTimerHandle;

    // === ī�޶� ���� �ý��� (������) ===

    // Ŭ���̾�Ʈ�κ��� ���� ī�޶� ������ �����丮
    UPROPERTY()
    TArray<FCameraPredictionData> ClientCameraHistory;

    // ���� ������ ī�޶� ����
    UPROPERTY()
    FCameraPredictionData PredictedClientCamera;

    // ���������� ���� Ŭ���̾�Ʈ ������
    UPROPERTY()
    FCameraPredictionData LastClientCamera;

    // ���� ��������
    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float MaxPredictionTime = 0.2f; // �ִ� ���� �ð� (200ms)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float CorrectionSpeed = 10.0f; // Ŭ���̾�Ʈ �����ͷ� �����ϴ� �ӵ�

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    int32 MaxHistorySize = 10; // ������ �����丮 ����

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionLocationThreshold = 100.0f; // ��� ������ ��ġ ���� �Ӱ谪 (cm)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionRotationThreshold = 10.0f; // ��� ������ ȸ�� ���� �Ӱ谪 (��)

    // ī�޶� ���� ���� �Լ���
    void UpdateClientCameraHistory(const struct FRepCamInfo& ClientCam);
    FCameraPredictionData PredictClientCameraMovement();
    FCameraPredictionData CorrectPredictionWithClientData(const FCameraPredictionData& Prediction, const struct FRepCamInfo& ClientData);
    void ApplyPredictedClientCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData);

    // ����׿� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugServerCameraPrediction();

//
        // === ī�޶� Proxy ���� ===
    UFUNCTION()
    void SpawnAndAttachCameraProxy(APlayerController* OwnerPC, APawn* OwnerPawn);

};
