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

    /** Skeletal Mesh - ī�޶� ���������� Mesh�� �ٿ� ��Ʈ��ũ ������ ���󰡰� �� */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USkeletalMeshComponent* SkeletalMesh;

    UPROPERTY()
    TObjectPtr<class USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* DummyCamera;

    // ���� ��ġ ��� ����ȭ
    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SyncWithRemotePlayer(FVector Location, FRotator Rotation);

    //  �ٸ� ĳ������ ī�޶� ���� ���󰡴� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SyncWithRemoteCamera(UCameraComponent* RemoteCamera);

    //  �ٸ� �÷��̾��� ���� �����ϰ� �ڵ����� ī�޶� ã��
    UFUNCTION(BlueprintCallable, Category = "Sync")
    void SetTargetPawn(APawn* TargetPawn);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync")
    float SmoothingSpeed = 8.0f;

    //  ī�޶� ����ȭ ��� ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Sync")
    bool bSyncDirectlyToCamera = true;

    // ī�޶� ������ ���� (bSyncDirectlyToCamera�� false�� �� ���)
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
    //  Ÿ�� ����
    UPROPERTY()
    APawn* TargetPawn = nullptr;

    UPROPERTY()
    UCameraComponent* TargetCamera = nullptr;

    // ���� ��ġ/ȸ�� ����ȭ��
    FVector TargetLocation;
    FRotator TargetRotation;
    bool bHasTarget = false;

    //  ���� �Լ���
    FVector CalculateOffsetPosition(const FVector& TargetPos, const FRotator& TargetRot);
    FRotator CalculateLookAtRotation(const FVector& FromPos, const FVector& ToPos);
    UCameraComponent* FindCameraInPawn(APawn* Pawn);
    void UpdateTargetCamera();

};
