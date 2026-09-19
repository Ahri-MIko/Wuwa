"""Inspect the current AnimBP; -WuwaConnectAnimation reparents it after backup.

Only connects the C++ data parent. Does not rewrite any EventGraph, states,
transitions, animation assets, or the character's mesh settings.
"""
import unreal

PATH = "/Game/Characters/Role/changli/AnimationBluePrint/ABP_Changli"
blueprint = unreal.load_asset(PATH)
if blueprint is None:
    raise RuntimeError("Cannot load " + PATH)
asset_data = unreal.EditorAssetLibrary.find_asset_data(PATH)
parent = asset_data.get_tag_value("ParentClass")
unreal.log("WuwaAnimation: parent=" + str(parent))

character_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Characters/Player/BP_WuwaCharacterBase")
character = unreal.get_default_object(character_class)
mesh = character.get_editor_property("mesh")
anim_class = mesh.get_editor_property("anim_class")
unreal.log("WuwaAnimation: character anim_class=" + (anim_class.get_path_name() if anim_class else "None"))

if "-WuwaConnectAnimation" in unreal.SystemLibrary.get_command_line():
    target = unreal.load_class(None, "/Script/Wuwa.WuwaAnimInstance")
    if target is None:
        raise RuntimeError("Build the Wuwa module before connecting animation")
    if not any(name in str(parent) for name in ("/Script/Engine.AnimInstance'", "/Script/Wuwa.WuwaAnimInstance'")):
        raise RuntimeError("Existing custom parent must be reviewed before reparenting; no asset saved")
    if "/Script/Wuwa.WuwaAnimInstance'" not in str(parent):
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, target)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    # C++ asset automation verifies ParentClass, GeneratedClass and compile Status.
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError("Could not save animation blueprint")
    unreal.log("WuwaAnimation: SAVED parent=WuwaAnimInstance; existing graphs preserved")
else:
    unreal.log("WuwaAnimation: INSPECTION ONLY; no assets changed")
