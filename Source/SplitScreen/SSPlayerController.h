// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;
class ASSCameraViewProxy;
class UCameraComponent;
class ACharacter;

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

    // === CharacterMovement ��� ����ȭ �ý��� ===

    // Ÿ�� ĳ���Ϳ� ���� ������Ʈ��
    UPROPERTY()
    TWeakObjectPtr<ACharacter> TargetCharacter;

    UPROPERTY()
    TWeakObjectPtr<UCharacterMovementComponent> TargetMovementComponent;

    UPROPERTY()
    TWeakObjectPtr<UCameraComponent> TargetCameraComponent;

    // ����ȭ ����
    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    float MovementSyncRate = 120.0f; // �ʴ� ����ȭ Ƚ��

    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    float CameraSmoothingSpeed = 15.0f; // ī�޶� ���� �ӵ�

    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    bool bUsePredictiveSync = true; // CharacterMovement�� prediction ��� ����

    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    float MaxSyncDistance = 1000.0f; // �ִ� ����ȭ �Ÿ� (�ʹ� �ָ� �ڷ���Ʈ)

    // ���� ������ ������ �����
    FVector LastKnownLocation;
    FRotator LastKnownRotation;
    FVector LastKnownVelocity;
    float LastSyncTime;

    // CharacterMovement ��� ����ȭ �Լ���
    void FindAndSetTargetCharacter();
    void SyncCameraWithCharacterMovement(ASSDummySpectatorPawn* DummyPawn);
    void ApplyCharacterMovementPrediction(ASSDummySpectatorPawn* DummyPawn);
    FVector PredictCharacterLocation(const FVector& CurrentLocation, const FVector& Velocity, float DeltaTime);
    void UpdateCameraFromCharacter(ASSDummySpectatorPawn* DummyPawn, const FVector& PredictedLocation, const FRotator& CharacterRotation);

    // ����׿� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugMovementSync();
};
