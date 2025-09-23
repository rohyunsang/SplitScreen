// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "SSDummySpectatorPawn.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSDummySpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()
	
public:
    ASSDummySpectatorPawn();

    /** Skeletal Mesh - 카메라 스프링암을 Mesh에 붙여 네트워크 보간을 따라가게 함 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USkeletalMeshComponent* SkeletalMesh;

    UPROPERTY()
    TObjectPtr<class USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* DummyCamera;

    // 기존 위치 기반 동기화
    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SyncWithRemotePlayer(FVector Location, FRotator Rotation);

    //  다른 캐릭터의 카메라를 직접 따라가는 함수
    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SyncWithRemoteCamera(UCameraComponent* RemoteCamera);

    //  다른 플레이어의 폰을 설정하고 자동으로 카메라 찾기
    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SetTargetPawn(APawn* TargetPawn);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync")
    float SmoothingSpeed = 8.0f;

    //  카메라 동기화 모드 선택
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Sync")
    bool bSyncDirectlyToCamera = true;

    // 카메라 오프셋 설정 (bSyncDirectlyToCamera가 false일 때 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Offset")
    FVector CameraOffset = FVector(-300.0f, 0.0f, 100.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Offset")
    bool bUseOffsetFromTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Offset")
    bool bLookAtTarget = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Offset")
    float MinDistanceFromTarget = 50.0f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    //  타겟 정보
    UPROPERTY()
    APawn* TargetPawn = nullptr;

    UPROPERTY()
    UCameraComponent* TargetCamera = nullptr;

    // 수동 위치/회전 동기화용
    FVector TargetLocation;
    FRotator TargetRotation;
    bool bHasTarget = false;

    //  헬퍼 함수들
    FVector CalculateOffsetPosition(const FVector& TargetPos, const FRotator& TargetRot);
    FRotator CalculateLookAtRotation(const FVector& FromPos, const FVector& ToPos);
    UCameraComponent* FindCameraInPawn(APawn* Pawn);
    void UpdateTargetCamera();

};
