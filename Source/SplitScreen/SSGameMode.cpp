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

ASSGameMode::ASSGameMode()
{
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
		// 정확히 2명(호스트+원격)일 때 세팅
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

		// 캐시 무효화
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
		GetWorldTimerManager().SetTimer(
			SyncTimerHandle,
			[this]() { SyncDummyPlayerWithProxy(); },
			0.0083f, // ~120Hz
			true
		);
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

		if (PC->IsLocalController())
		{
			HostPC = PC;   // 리슨서버 로컬
		}
		else
		{
			ClientPC = PC; // 원격 클라
		}
	}

	// **중요: 캐시**
	CachedHostPC = HostPC;
	CachedRemotePC = ClientPC;

	UE_LOG(LogTemp, Warning, TEXT("SS Found Host PC: %s, Client PC: %s"),
		HostPC ? *HostPC->GetName() : TEXT("NULL"),
		ClientPC ? *ClientPC->GetName() : TEXT("NULL"));

	// 서버 POV 프록시
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

	// 클라 POV 프록시
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
		// return;
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

	const FVector SpawnLocation(0, 0, 0);
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
	ApplyProxyCamera(DummySpectatorPawn, ClientCam);
}

ACharacter* ASSGameMode::FindRemoteCharacter() const
{
	// 1) 가장 신뢰도 높은 경로: **캐시된 RemotePC의 Pawn**
	if (CachedRemotePC.IsValid())
	{
		if (APawn* Pawn = CachedRemotePC->GetPawn())
		{
			if (ACharacter* Char = Cast<ACharacter>(Pawn))
			{
				return Char;
			}
		}
	}

	// 2) ConnectedPlayers에서 원격 PC 우선
	for (APlayerController* PC : ConnectedPlayers)
	{
		if (!PC) continue;
		if (!PC->IsLocalController()) // 원격
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (ACharacter* Char = Cast<ACharacter>(Pawn))
				{
					return Char;
				}
			}
		}
	}

	// 3) 폴백: 월드 순회 (더미/호스트 제외)
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Char = *It;
		if (!Char) continue;

		AController* Ctrl = Char->GetController();
		if (!Ctrl) continue;

		// 더미 컨트롤러/호스트 컨트롤러는 제외
		if (Ctrl == DummyPlayerController) continue;
		if (CachedHostPC.IsValid() && Ctrl == CachedHostPC.Get()) continue;

		// 원격일 가능성이 높은 캐릭터
		return Char;
	}

	// 4) 마지막 폴백 없음
	return nullptr;
}

void ASSGameMode::ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData)
{
	if (!DummyPawn || !DummyPlayerController) return;

	ACharacter* TargetChar = CachedRemoteCharacter.Get();
	if (!TargetChar)
	{
		TargetChar = FindRemoteCharacter();
		CachedRemoteCharacter = TargetChar;
	}
	if (!TargetChar) return; // 아직 원격 Pawn이 안 붙은 프레임이면 다음 틱에 재시도

	FVector Pivot = TargetChar->GetActorLocation();
	Pivot.Z += PivotZOffset;

	const FVector CurLoc = DummyPawn->GetActorLocation();
	const float   Delta = GetWorld()->GetDeltaSeconds();
	const float   Dist = FVector::Dist(CurLoc, Pivot);

	if (Dist > ServerSnapDistance)
	{
		DummyPawn->SetActorLocation(Pivot);
		DummyPlayerController->SetControlRotation(CamData.Rotation);
		UE_LOG(LogTemp, Verbose, TEXT("SS(Server) snap to pivot (dist=%.1f)"), Dist);
	}
	else
	{
		const FVector  NewLoc = FMath::VInterpTo(CurLoc, Pivot, Delta, ServerLocationInterpSpeed);
		const FRotator CurCtrl = DummyPlayerController->GetControlRotation();
		const FRotator NewCtrl = FMath::RInterpTo(CurCtrl, CamData.Rotation, Delta, ServerRotationInterpSpeed);

		DummyPawn->SetActorLocation(NewLoc);
		DummyPlayerController->SetControlRotation(NewCtrl);
	}

	if (UCameraComponent* Cam = DummyPawn->FindComponentByClass<UCameraComponent>())
	{
		Cam->SetFieldOfView(CamData.FOV);
	}
}

void ASSGameMode::UpdateSplitScreenLayout()
{
	UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Layout Updated"));
}
