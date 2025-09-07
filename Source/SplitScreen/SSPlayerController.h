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

// 카메라 예측을 위한 데이터 구조체
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

    // 더미 컨트롤러 플래그
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bIsDummyController = false;

    // 더미 컨트롤러로 설정
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetAsDummyController(bool bDummy = true);

    // 캐시된 프록시 참조
    UPROPERTY()
    TWeakObjectPtr<ASSCameraViewProxy> CachedProxy;

protected:
    // 네트워크 RPC 함수들
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // 입력 설정
    virtual void SetupInputComponent() override;

private:
    // 네트워크 업데이트 관련
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // 클라이언트용 함수들
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // 클라이언트 더미 관련 변수들
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;

    // === CharacterMovement 기반 동기화 시스템 ===

    // 타겟 캐릭터와 관련 컴포넌트들
    UPROPERTY()
    TWeakObjectPtr<ACharacter> TargetCharacter;

    UPROPERTY()
    TWeakObjectPtr<UCharacterMovementComponent> TargetMovementComponent;

    UPROPERTY()
    TWeakObjectPtr<UCameraComponent> TargetCameraComponent;

    // 동기화 설정
    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    float MovementSyncRate = 120.0f; // 초당 동기화 횟수

    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    float CameraSmoothingSpeed = 15.0f; // 카메라 보간 속도

    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    bool bUsePredictiveSync = true; // CharacterMovement의 prediction 사용 여부

    UPROPERTY(EditAnywhere, Category = "Movement Sync")
    float MaxSyncDistance = 1000.0f; // 최대 동기화 거리 (너무 멀면 텔레포트)

    // 이전 프레임 데이터 저장용
    FVector LastKnownLocation;
    FRotator LastKnownRotation;
    FVector LastKnownVelocity;
    float LastSyncTime;

    // CharacterMovement 기반 동기화 함수들
    void FindAndSetTargetCharacter();
    void SyncCameraWithCharacterMovement(ASSDummySpectatorPawn* DummyPawn);
    void ApplyCharacterMovementPrediction(ASSDummySpectatorPawn* DummyPawn);
    FVector PredictCharacterLocation(const FVector& CurrentLocation, const FVector& Velocity, float DeltaTime);
    void UpdateCameraFromCharacter(ASSDummySpectatorPawn* DummyPawn, const FVector& PredictedLocation, const FRotator& CharacterRotation);

    // 디버그용 함수
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugMovementSync();
};
