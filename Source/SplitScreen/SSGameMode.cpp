// Fill out your copyright notice in the Description page of Project Settings.

#include "SSGameMode.h"
#include "SSGameInstance.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "SSCameraViewProxy.h"

#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	// 컷오프(Hz) → 프레임 독립적인 지수 보간 계수(0~1)로 변환
	FORCEINLINE float CutoffToAlpha(float CutoffHz, float Dt)
	{
		const float K = 2.f * PI * CutoffHz;
		return 1.f - FMath::Exp(-K * Dt);
	}

	// 쿼터니언 간 각도차(도) ? 데드존 판단용
	FORCEINLINE float QuatDeltaDegrees(const FQuat& A, const FQuat& B)
	{
		const float Dot = FMath::Clamp(A | B, -1.f, 1.f);
		return FMath::RadiansToDegrees(2.f * FMath::Acos(Dot));
	}
}

// ─────────────────────────────────────────────────────────────────────

ASSGameMode::ASSGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerControllerClass = ASSPlayerController::StaticClass();
	DummySpectatorPawnClass = ASSDummySpectatorPawn::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void ASSGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoEnableSplitScreen)
	{
		if (USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance()))
		{
			SSGI->EnableSplitScreen();
		}
	}
}

void ASSGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DummySpectatorPawn && (CachedClientProxy.IsValid() || ClientCamProxy))
	{
		SyncDummyPlayerWithProxy(); // ★ 프레임에 한 번만
	}
}

void ASSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ConnectedPlayers.AddUnique(NewPlayer);

	const FString NetModeString = (GetWorld()->GetNetMode() == NM_ListenServer)
		? TEXT("ListenServer") : TEXT("Client");

	UE_LOG(LogTemp, Warning, TEXT("SS PostLogin: %s, Local: %s, Total: %d, NetMode: %s"),
		*NewPlayer->GetName(),
		NewPlayer->IsLocalController() ? TEXT("Yes") : TEXT("No"),
		ConnectedPlayers.Num(),
		*NetModeString);

	if (bAutoEnableSplitScreen && GetWorld()->GetNetMode() == NM_ListenServer)
	{
		if (ConnectedPlayers.Num() == 2 && !DummyPlayerController)
		{
			UE_LOG(LogTemp, Warning, TEXT("SS Starting split screen setup..."));
			SetupOnlineSplitScreen();
		}
	}
}

void ASSGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		ConnectedPlayers.Remove(PC);
		if (CachedRemotePC.Get() == PC) { CachedRemotePC = nullptr; }
		if (CachedHostPC.Get() == PC) { CachedHostPC = nullptr; }
	}
	Super::Logout(Exiting);
}

void ASSGameMode::SetupOnlineSplitScreen()
{
	USSGameInstance* SSGI = Cast<USSGameInstance>(GetGameInstance());
	if (!SSGI || !SSGI->IsSplitScreenEnabled())
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Split screen not enabled in game instance"));
		return;
	}

	if (DummyPlayerController && DummySpectatorPawn && DummyPlayerController->GetLocalPlayer())
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Split screen already fully setup"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("SS Setting up online split screen..."));

	CreateDummyLocalPlayer();
	UpdateSplitScreenLayout();
	SetupCameraProxies();

	if (DummyPlayerController && DummySpectatorPawn && ClientCamProxy)
	{
		/*GetWorldTimerManager().SetTimer(
			SyncTimerHandle,
			[this]() { SyncDummyPlayerWithProxy(); },
			0.0083f, // ~120Hz
			true
		);
		*/
		UE_LOG(LogTemp, Warning, TEXT("SS Split screen setup completed successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SS Split screen setup failed"));
	}
}

void ASSGameMode::SetupCameraProxies()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APlayerController* HostPC = nullptr;
	APlayerController* ClientPC = nullptr;

	for (APlayerController* PC : ConnectedPlayers)
	{
		if (!PC || PC == DummyPlayerController) continue;

		if (PC->IsLocalController()) HostPC = PC; // 리슨 서버 로컬
		else                        ClientPC = PC; // 원격 클라
	}

	CachedHostPC = HostPC;
	CachedRemotePC = ClientPC;

	UE_LOG(LogTemp, Warning, TEXT("SS Found Host PC: %s, Client PC: %s"),
		HostPC ? *HostPC->GetName() : TEXT("NULL"),
		ClientPC ? *ClientPC->GetName() : TEXT("NULL"));

	if (!ServerCamProxy)
	{
		ServerCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
			ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
		if (ServerCamProxy && HasAuthority() && HostPC)
		{
			ServerCamProxy->SetSourcePC(HostPC);
			UE_LOG(LogTemp, Warning, TEXT("SS Created ServerCamProxy with Host PC"));
		}
	}

	if (!ClientCamProxy)
	{
		ClientCamProxy = GetWorld()->SpawnActor<ASSCameraViewProxy>(
			ASSCameraViewProxy::StaticClass(), FTransform::Identity, Params);
		if (ClientCamProxy && HasAuthority() && ClientPC)
		{
			ClientCamProxy->SetSourcePC(ClientPC);
			CachedClientProxy = ClientCamProxy;
			UE_LOG(LogTemp, Warning, TEXT("SS Created ClientCamProxy with Client PC"));
		}
	}
}

void ASSGameMode::CreateDummyLocalPlayer()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	const int32 CurrentLocalPlayers = GI->GetNumLocalPlayers();
	if (CurrentLocalPlayers >= 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Already have 2+ local players"));
	}

	FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
	FString OutError;
	ULocalPlayer* DummyLP = GI->CreateLocalPlayer(DummyUserId, OutError, false);
	if (!DummyLP)
	{
		UE_LOG(LogTemp, Error, TEXT("SS Failed to create dummy local player (%s)"), *OutError);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("SS Success to create dummy local player"));

	const FVector  SpawnLocation(0, 0, 0);
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	DummySpectatorPawn = GetWorld()->SpawnActor<ASSDummySpectatorPawn>(
		DummySpectatorPawnClass ? *DummySpectatorPawnClass : ASSDummySpectatorPawn::StaticClass(),
		SpawnLocation, SpawnRotation);

	if (!DummySpectatorPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("SS Failed to spawn dummy spectator pawn"));
		return;
	}

	DummyPlayerController = GetWorld()->SpawnActor<ASSPlayerController>();
	if (DummyPlayerController)
	{
		DummyPlayerController->SetAsDummyController(true);
		DummyPlayerController->SetPawn(nullptr);
		DummyPlayerController->SetPlayer(DummyLP);
		DummyPlayerController->Possess(DummySpectatorPawn);

		UE_LOG(LogTemp, Warning, TEXT("SS Dummy Local Player Created Successfully"));
	}
}

void ASSGameMode::SyncDummyPlayerWithProxy()
{
	if (!DummySpectatorPawn) return;

	ASSCameraViewProxy* Proxy = CachedClientProxy.Get();
	if (!Proxy)
	{
		Proxy = ClientCamProxy;
		if (Proxy) CachedClientProxy = Proxy;
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SS ClientCamProxy not found"));
			return;
		}
	}

	const FRepCamInfo& ClientCam = Proxy->GetReplicatedCamera();

	// 1) 새로운 스냅샷이면 히스토리 갱신
	bool bNewData = false;
	if (!LastClientCamera.Location.Equals(ClientCam.Location, 1.0f) ||
		!LastClientCamera.Rotation.Equals(ClientCam.Rotation, 1.0f))
	{
		bNewData = true;
		UpdateCameraHistory(ClientCam);
	}

	// 2) 예측
	FCameraPredictionDataGM Predicted = PredictCameraMovement();

	// 3) 최신 스냅샷으로 보정
	if (bNewData)
	{
		Predicted = CorrectPredictionWithServerData(Predicted, ClientCam);
	}

	// 4) 적용
	ApplyPredictedCamera(DummySpectatorPawn, Predicted);
}

ACharacter* ASSGameMode::FindRemoteCharacter() const
{
	// 1) 캐시된 원격 PC
	if (CachedRemotePC.IsValid())
	{
		if (APawn* Pawn = CachedRemotePC->GetPawn())
			if (ACharacter* Char = Cast<ACharacter>(Pawn))
				return Char;
	}

	// 2) ConnectedPlayers에서 원격
	for (APlayerController* PC : ConnectedPlayers)
	{
		if (!PC) continue;
		if (!PC->IsLocalController())
		{
			if (APawn* Pawn = PC->GetPawn())
				if (ACharacter* Char = Cast<ACharacter>(Pawn))
					return Char;
		}
	}

	// 3) 폴백: 월드 순회
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Char = *It;
		if (!Char) continue;

		AController* Ctrl = Char->GetController();
		if (!Ctrl) continue;
		if (Ctrl == DummyPlayerController) continue;
		if (CachedHostPC.IsValid() && Ctrl == CachedHostPC.Get()) continue;

		return Char;
	}
	return nullptr;
}


// ───────────── 예측 로직 ─────────────

void ASSGameMode::UpdateCameraHistory(const FRepCamInfo& ClientCam)
{
	FCameraPredictionDataGM NewData;
	NewData.Location = ClientCam.Location;
	NewData.Rotation = ClientCam.Rotation;
	NewData.FOV = ClientCam.FOV;
	NewData.SpringArmLength = ClientCam.SpringArmLength;
	NewData.Timestamp = GetWorld()->GetTimeSeconds();

	// 속도/각속도
	if (CameraHistory.Num() > 0)
	{
		const FCameraPredictionDataGM& Last = CameraHistory.Last();
		const float Dt = FMath::Max(NewData.Timestamp - Last.Timestamp, 0.f);
		if (Dt > 0.f)
		{
			NewData.Velocity = (NewData.Location - Last.Location) / Dt;
			const FRotator DRot = (NewData.Rotation - Last.Rotation).GetNormalized();
			NewData.AngularVelocity = FVector(DRot.Pitch, DRot.Yaw, DRot.Roll) / Dt;
		}
	}

	CameraHistory.Add(NewData);
	if (CameraHistory.Num() > MaxHistorySize)
	{
		CameraHistory.RemoveAt(0);
	}

	LastClientCamera = NewData;
}

FCameraPredictionDataGM ASSGameMode::PredictCameraMovement()
{
	if (CameraHistory.Num() == 0) return PredictedCamera;

	const FCameraPredictionDataGM& Latest = CameraHistory.Last();
	const float Now = GetWorld()->GetTimeSeconds();
	const float PredDt = FMath::Clamp(Now - Latest.Timestamp, 0.f, MaxPredictionTime);

	FCameraPredictionDataGM Pred = Latest;

	if (PredDt > 0.f && CameraHistory.Num() >= 2)
	{
		// 선형 + (있으면) 가속도
		Pred.Location = Latest.Location + Latest.Velocity * PredDt;

		if (CameraHistory.Num() >= 3)
		{
			const FCameraPredictionDataGM& Prev = CameraHistory[CameraHistory.Num() - 2];
			const float Den = FMath::Max(Latest.Timestamp - Prev.Timestamp, 0.001f);
			const FVector Accel = (Latest.Velocity - Prev.Velocity) / Den;
			Pred.Location += 0.5f * Accel * PredDt * PredDt;
		}

		// 회전 예측은 지터 때문에 생략 → 최신 스냅샷 유지
		Pred.Rotation = Latest.Rotation;
	}
	else
	{
		Pred.Location = Latest.Location;
		Pred.Rotation = Latest.Rotation;
	}

	Pred.FOV = Latest.FOV;
	Pred.SpringArmLength = Latest.SpringArmLength;
	PredictedCamera = Pred;
	return Pred;
}

FCameraPredictionDataGM ASSGameMode::CorrectPredictionWithServerData(const FCameraPredictionDataGM& Prediction,
	const FRepCamInfo& ClientCam)
{
	FCameraPredictionDataGM Corrected = Prediction;

	const FVector  LocErr = ClientCam.Location - Prediction.Location;
	const FRotator RotErr = (ClientCam.Rotation - Prediction.Rotation).GetNormalized();

	const float LocMag = LocErr.Size();
	const float RotMag = FMath::Abs(RotErr.Yaw) + FMath::Abs(RotErr.Pitch);

	const float Dt = GetWorld()->GetDeltaSeconds();

	// 위치: 큰 오차는 스냅, 아니면 보간
	if (LocMag > 100.f)
	{
		Corrected.Location = ClientCam.Location;
	}
	else
	{
		Corrected.Location = FMath::VInterpTo(Prediction.Location, ClientCam.Location, Dt, CorrectionSpeed);
	}

	// 회전: 큰 오차는 스냅, 아니면 보간
	if (RotMag > 10.f)
	{
		Corrected.Rotation = ClientCam.Rotation;
	}
	else
	{
		Corrected.Rotation = FMath::RInterpTo(Prediction.Rotation, ClientCam.Rotation, Dt, CorrectionSpeed);
	}

	Corrected.FOV = ClientCam.FOV;
	return Corrected;
}

void ASSGameMode::ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionDataGM& Cam)
{
	if (!DummyPawn || !DummyPlayerController) return;

	// 0) 더미 스프링암: 래그/충돌 OFF, 길이 일치
	if (USpringArmComponent* Boom = DummyPawn->FindComponentByClass<USpringArmComponent>())
	{
		Boom->bEnableCameraLag = false;
		Boom->bEnableCameraRotationLag = false;
		Boom->bDoCollisionTest = false;
		Boom->TargetArmLength = Cam.SpringArmLength; // 클라와 동일 암 길이
		Boom->SocketOffset = FVector::ZeroVector;    // (클라에서 사용 중이면 복제/적용)
	}

	// 1) 카메라에서 피벗 역산 (회전-피벗 커플링 보정은 아래 저역통과가 줄여줌)
	const FVector Forward = Cam.Rotation.Vector();
	const FVector Pivot = Cam.Location + Forward * Cam.SpringArmLength;

	// 2) 위치 저역통과 (프레임 독립, 15~25Hz 권장)
	const float Dt = GetWorld()->GetDeltaSeconds();
	const float LocAlpha = CutoffToAlpha(20.f /*Hz*/, Dt);
	const FVector CurLoc = DummyPawn->GetActorLocation();
	const FVector NewLoc = FMath::Lerp(CurLoc, Pivot, LocAlpha);
	DummyPawn->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

	// 3) 회전 SLERP + 지수 감쇠 (10~14Hz 권장) + 데드존 스냅
	if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
	{
		const FQuat CurQ = DummyController->GetControlRotation().Quaternion();
		const FQuat TgtQ = Cam.Rotation.Quaternion();
		const float RotAlpha = CutoffToAlpha(12.f /*Hz*/, Dt);

		// 데드존: 아주 작은 차이는 바로 스냅해서 떨림 제거
		const float DeltaDeg = QuatDeltaDegrees(CurQ, TgtQ);
		if (DeltaDeg < 0.15f) // 0.1~0.2도 사이 추천
		{
			DummyController->SetControlRotation(Cam.Rotation);
		}
		else
		{
			const FQuat NewQ = FQuat::Slerp(CurQ, TgtQ, RotAlpha).GetNormalized();
			DummyController->SetControlRotation(NewQ.Rotator());
		}
	}

	// 4) FOV 즉시 적용
	if (UCameraComponent* Camera = DummyPawn->FindComponentByClass<UCameraComponent>())
	{
		Camera->SetFieldOfView(Cam.FOV);
	}
}


void ASSGameMode::ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData)
{
	// 헤더에 존재하던 선언을 실제 구현으로 제공 (예측 파이프라인 래퍼)
	UpdateCameraHistory(CamData);
	FCameraPredictionDataGM Pred = PredictCameraMovement();
	Pred = CorrectPredictionWithServerData(Pred, CamData);
	ApplyPredictedCamera(DummyPawn, Pred);
}

void ASSGameMode::UpdateSplitScreenLayout()
{
	UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}
