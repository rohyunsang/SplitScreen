// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSGameMode.generated.h"

struct FRepCamInfo;
class ASSDummySpectatorPawn;
class ASSPlayerController;
class ASSCameraViewProxy;
class ACharacter;

// ī�޶� ������ ���� ������ ����ü
USTRUCT(BlueprintType)
struct FCameraPredictionDataGM
{
	GENERATED_BODY()

	UPROPERTY() FVector  Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float    FOV = 90.0f;
	float    SpringArmLength = 0.f;

	// ������
	UPROPERTY() FVector  Velocity = FVector::ZeroVector;
	UPROPERTY() FVector  AngularVelocity = FVector::ZeroVector;
	UPROPERTY() float    Timestamp = 0.0f;
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
	virtual void Tick(float DeltaTime) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
	bool bAutoEnableSplitScreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
	TSubclassOf<APawn> DummySpectatorPawnClass;

protected:
	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void SetupOnlineSplitScreen();

	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void UpdateSplitScreenLayout();

private:
	// ������ ���� ��Ʈ��ũ �÷��̾��(���� ����)
	UPROPERTY(Transient)
	TArray<APlayerController*> ConnectedPlayers;

	// ���� 2P ȭ��� ����
	UPROPERTY(Transient)
	ASSDummySpectatorPawn* DummySpectatorPawn = nullptr;

	UPROPERTY(Transient)
	ASSPlayerController* DummyPlayerController = nullptr;

	// ī�޶� ���Ͻ� (GC ��ȣ)
public:
	UPROPERTY(Transient)
	ASSCameraViewProxy* ServerCamProxy = nullptr;

	UPROPERTY(Transient)
	ASSCameraViewProxy* ClientCamProxy = nullptr;

private:
	// ����ȭ/���Ͻ�/Ž��
	void CreateDummyLocalPlayer();
	void SetupCameraProxies();
	void SyncDummyPlayerWithProxy();
	ACharacter* FindRemoteCharacter() const;

	// ���� (���� ����)
	void ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionDataGM& CameraData);

	// ȣȯ��: ����� �ִ� ������ ���� �������� ä����(���� ���������� ����)
	void ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData);

	FTimerHandle SyncTimerHandle;

	// ĳ��
	TWeakObjectPtr<ASSCameraViewProxy> CachedClientProxy;
	TWeakObjectPtr<ACharacter>         CachedRemoteCharacter;
	TWeakObjectPtr<APlayerController>  CachedRemotePC;
	TWeakObjectPtr<APlayerController>  CachedHostPC;

	// Ʃ�� �Ķ����
	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerSnapDistance = 250.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerLocationInterpSpeed = 18.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerRotationInterpSpeed = 28.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float PivotZOffset = 60.f;

	// ������������������ ������ ����/Ʃ�� ������������������
	// �����丮
	UPROPERTY(Transient)
	TArray<FCameraPredictionDataGM> CameraHistory;

	// ���������� ������ Ŭ�� ī�޶�(��õ)
	UPROPERTY(Transient)
	FCameraPredictionDataGM LastClientCamera;

	// �ֽ� ���� ���
	UPROPERTY(Transient)
	FCameraPredictionDataGM PredictedCamera;

	// ���� Ʃ��
	UPROPERTY(EditAnywhere, Category = "Split Screen|Prediction")
	int32 MaxHistorySize = 10;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Prediction")
	float MaxPredictionTime = 0.06f;   // �� (��Ʈ ���� ������)

	UPROPERTY(EditAnywhere, Category = "Split Screen|Prediction")
	float CorrectionSpeed = 20.f;      // ���� ���� �ӵ�

	// ���� ����
	void UpdateCameraHistory(const FRepCamInfo& ClientCam);
	FCameraPredictionDataGM PredictCameraMovement();
	FCameraPredictionDataGM CorrectPredictionWithServerData(const FCameraPredictionDataGM& Prediction,
		const FRepCamInfo& ClientCam);
	
};
