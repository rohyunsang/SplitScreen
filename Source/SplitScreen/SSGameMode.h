// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "SSGameMode.generated.h"

struct FCameraPredictionData;

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

private:
    TArray<APlayerController*> ConnectedPlayers;
    class ASSDummySpectatorPawn* DummySpectatorPawn;
    class ASSPlayerController* DummyPlayerController;

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
    float MaxPredictionTime = 0.03f; // �ִ� ���� �ð� (200ms)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float CorrectionSpeed = 30.f; // Ŭ���̾�Ʈ �����ͷ� �����ϴ� �ӵ�

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

};
