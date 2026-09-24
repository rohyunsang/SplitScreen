# Verifies the content layout for Fab submission. Run with the editor CLOSED:
#   UnrealEditor-Cmd.exe SplitScreen.uproject -run=pythonscript -script="Tools/VerifyContent.py" -EnablePlugins=PythonScriptPlugin -unattended -nosplash
# Prints [Verify] lines; "PROBLEM" lines must be fixed before submitting.

import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary

ROOTS = ["/Game", "/DynamicSplitScreen"]
PROJECT_TOP = "/Game/DynamicSplitScreenDemo"
MAPS = ["/Game/DynamicSplitScreenDemo/Maps/DynamicSplitScreenMap"]
EXPECTED_DEMO_VOLUMES = 7   # 2 Fixed, 2 Zoom, 2 Follow, 1 Merge

problems = 0


def log(msg):
    unreal.log("[Verify] " + msg)


def problem(msg):
    global problems
    problems += 1
    unreal.log_warning("[Verify] PROBLEM " + msg)


def main():
    registry.scan_paths_synchronous(ROOTS, True)
    opts = unreal.AssetRegistryDependencyOptions()

    packages = set()
    for root in ROOTS:
        for a in registry.get_assets_by_path(root, recursive=True, include_only_on_disk_assets=True):
            packages.add(str(a.package_name))
    log("%d packages" % len(packages))

    for pkg in sorted(packages):
        is_external = "/__External" in pkg

        # Layout: project content under one top folder, no redirectors
        if pkg.startswith("/Game/") and not is_external and not pkg.startswith(PROJECT_TOP + "/"):
            problem("outside top folder: " + pkg)

        for d in registry.get_dependencies(pkg, opts) or []:
            dep = str(d)
            if not dep.startswith(("/Game/", "/DynamicSplitScreen/")):
                continue
            if dep not in packages:
                problem("%s -> missing %s" % (pkg, dep))
            if pkg.startswith("/DynamicSplitScreen/") and dep.startswith("/Game/"):
                problem("plugin depends on project: %s -> %s" % (pkg, dep))

    redirector_class = unreal.TopLevelAssetPath("/Script/CoreUObject", "ObjectRedirector")
    for a in registry.get_assets(unreal.ARFilter(class_paths=[redirector_class], package_paths=ROOTS, recursive_paths=True)):
        problem("redirector " + str(a.package_name))

    for m in MAPS:
        world = unreal.EditorLoadingAndSavingUtils.load_map(m)
        if not world:
            problem("cannot load " + m)
            continue
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
        log("%s: %d actor descs" % (m, len(descs)))
        volumes = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                   if a.get_class().get_path_name().startswith("/DynamicSplitScreen/")]
        for a in volumes:
            log("  %s (%s)" % (a.get_actor_label(), a.get_class().get_path_name()))
        if len(volumes) != EXPECTED_DEMO_VOLUMES:
            problem("%s: expected %d plugin volumes, found %d" % (m, EXPECTED_DEMO_VOLUMES, len(volumes)))
        gm = world.get_world_settings().get_editor_property("default_game_mode")
        log("%s: GameMode override = %s" % (m, gm.get_path_name() if gm else None))

    gm_bp = eal.load_asset(PROJECT_TOP + "/Blueprints/BP_DynamicSplitScreenGameMode")
    if gm_bp:
        cdo = unreal.get_default_object(gm_bp.generated_class())
        pawn = cdo.get_editor_property("default_pawn_class")
        pc = cdo.get_editor_property("player_controller_class")
        log("GameMode BP: pawn=%s pc=%s" % (pawn.get_path_name() if pawn else None, pc.get_path_name() if pc else None))

    log("DONE problems=%d" % problems)


main()
