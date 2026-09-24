# Unreal Multi-Split Screen

A multi-split screen system for Unreal Engine  
well-suited for cooperative multiplayer games — in the spirit of *It Takes Two*.  

- Unreal 5.8.3

- Youtube
https://www.youtube.com/watch?v=Sr370iMZrtE

## How it works

Online (listen server) two-player split screen, where **both** machines show both players.

- Only one LocalPlayer per machine. The other player's view is rendered by injecting a second
  `FSceneView` into the ViewFamily (`UDynamicSplitScreenViewportClient`) — no dummy
  LocalPlayer / PlayerController / SpectatorPawn.
- `ADynamicSplitScreenCameraProxy` replicates each player's camera (location / rotation / FOV / arm length).
- `UDynamicSplitScreenSubsystem` smooths the other player's camera every frame, sweeps it against
  collision like a spring arm, and drives split <-> full screen transitions.

## Setup

1. Project Settings > Engine > General Settings > **Game Viewport Client Class** = `DynamicSplitScreenViewportClient`
2. Inherit your GameMode from `ADynamicSplitScreenGameMode`
   and your PlayerController from `ADynamicSplitScreenPlayerController`.
3. Your character needs a `USpringArmComponent` + `UCameraComponent` (the standard third-person setup).

## Level actors

| Actor | Purpose |
|---|---|
| `SplitScreenTransitionTrigger` | Local player inside -> this screen goes full screen |
| `SplitScreenMergeVolume` | N players inside -> merge into a single view |
| `FixedCameraVolume` | Fixed camera angle, optional full screen |
| `FollowCameraVolume` | Camera follows at a fixed offset, optional full screen |
| `CameraZoomVolume` | Changes spring arm length, optional full screen |

Full screen requests are reference counted per requester, so overlapping volumes hand over cleanly.

## Tuning

Smoothing and collision values can be overridden in `DefaultGame.ini`:

```ini
[/Script/DynamicSplitScreen.DynamicSplitScreenSubsystem]
TransitionDuration=0.5
RotationInterpSpeed=45
AnchorInterpSpeed=60
bMainViewOnRight=True
```

Debug: `DynamicSplitScreen.DebugDraw 1` draws the secondary camera.
