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

protected:
	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void SetupOnlineSplitScreen();

	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void UpdateSplitScreenLayout();

private:
	// ������ ���� ��Ʈ��ũ �÷��̾��(���� ����)
	UPROPERTY(Transient)
	TArray<APlayerController*> ConnectedPlayers;

	// ���� ����Ʈ�� 2P ����
	UPROPERTY(Transient)
	ASSDummySpectatorPawn* DummySpectatorPawn = nullptr;

	UPROPERTY(Transient)
	ASSPlayerController* DummyPlayerController = nullptr;

	// ī�޶� ����ȭ
	void CreateDummyLocalPlayer();
	void SyncDummyPlayerWithProxy();
	void ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData);
	void SetupCameraProxies();

	FTimerHandle SyncTimerHandle;

public:
	// ī�޶� ���Ͻ� (GC ��ȣ)
	UPROPERTY(Transient)
	ASSCameraViewProxy* ServerCamProxy = nullptr;

	UPROPERTY(Transient)
	ASSCameraViewProxy* ClientCamProxy = nullptr;

private:
	// ĳ��
	TWeakObjectPtr<ASSCameraViewProxy>   CachedClientProxy;
	TWeakObjectPtr<ACharacter>           CachedRemoteCharacter;

	// **�ٽ�: ����/ȣ��Ʈ PC ĳ��**
	TWeakObjectPtr<APlayerController>    CachedRemotePC;
	TWeakObjectPtr<APlayerController>    CachedHostPC;

	// Ʃ�� �Ķ����
	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerSnapDistance = 250.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerLocationInterpSpeed = 18.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerRotationInterpSpeed = 28.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float PivotZOffset = 60.f;

private:
	// **���� ĳ���� Ž�� (���� �ŷڵ� ���� ��κ���)**
	ACharacter* FindRemoteCharacter() const;
};
