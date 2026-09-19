"""UE editor commandlet helper for the existing Alt input assets.

Read-only by default. Pass -WuwaApplyWalkRun on the command line to save
only IMC_Character and DA_InputActionTagAsset. Back up both assets first.
"""

import unreal


ACTION_PATH = "/Game/CoreInput/Actions/IA_WalkRun"
CONTEXT_PATH = "/Game/CoreInput/Contexts/IMC_Character"
TABLE_PATH = "/Game/DataAsset/DA_InputActionTagAsset"
INPUT_TAG = "Player.Common.Movement.WalkRun"
ROUTE_TAG = "Input.Route.Movement"


def require_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: " + path)
    return asset


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="' + name + '")'):
        raise RuntimeError("Cannot import gameplay tag: " + name)
    return tag


action = require_asset(ACTION_PATH)
context = require_asset(CONTEXT_PATH)
table = require_asset(TABLE_PATH)
mappings = list(context.get_editor_property("default_key_mappings").get_editor_property("mappings"))
rows = list(table.get_editor_property("input_data_asset_map"))

unreal.log("WuwaWalkRun: action value type=" + str(action.get_editor_property("value_type")))
for mapping in mappings:
    mapped_action = mapping.get_editor_property("action")
    if mapped_action == action:
        unreal.log("WuwaWalkRun: existing mapping " + mapping.export_text())
for row in rows:
    unreal.log("WuwaWalkRun: existing binding " + row.export_text())

# Confirm that the actual project controller uses the table we are editing.
controller_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Core/BP_WuwaPlayerController")
controller = unreal.get_default_object(controller_class)
if controller.get_editor_property("input_tag_map") != table:
    raise RuntimeError("BP_WuwaPlayerController does not use DA_InputActionTagAsset; no assets saved")

if "-WuwaApplyWalkRun" in unreal.SystemLibrary.get_command_line():
    input_tag = make_tag(INPUT_TAG)
    route_tag = make_tag(ROUTE_TAG)
    matching_rows = [row for row in rows if row.get_editor_property("input_action") == action]
    if len(matching_rows) > 1:
        raise RuntimeError("Duplicate IA_WalkRun bindings; no assets saved")
    for row in rows:
        if row.get_editor_property("input_tag") == input_tag and row.get_editor_property("input_action") != action:
            raise RuntimeError("WalkRun tag already belongs to another action; no assets saved")

    # Preserve all other IA mappings and routes. Do not create a second input path.
    row = matching_rows[0] if matching_rows else unreal.InputDataAsset()
    row.set_editor_property("input_action", action)
    row.set_editor_property("input_tag", input_tag)
    row.set_editor_property("route_tag", route_tag)
    if not matching_rows:
        rows.append(row)
    table.set_editor_property("input_data_asset_map", rows)

    alt_key = unreal.Key()
    # FKey has a custom text serializer: it accepts "LeftAlt", not a struct tuple.
    if not alt_key.import_text("LeftAlt") or alt_key.export_text() != "LeftAlt":
        raise RuntimeError("Cannot construct LeftAlt")
    existing_alt = [mapping for mapping in mappings
                    if mapping.get_editor_property("action") == action
                    and mapping.get_editor_property("key") == alt_key]
    if len(existing_alt) > 1:
        raise RuntimeError("Duplicate LeftAlt mappings; no assets saved")
    # The existing WalkRun action used LeftControl. Migrate that key to Alt,
    # without touching any other action or non-keyboard mapping.
    old_key = unreal.Key()
    old_key.import_text("LeftControl")
    context.unmap_key(action, old_key)
    if not existing_alt:
        context.map_key(action, alt_key)

    for asset in (context, table):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("Could not save " + asset.get_path_name())
    unreal.log("WuwaWalkRun: SAVED Alt -> IA_WalkRun -> Player.Common.Movement.WalkRun -> Input.Route.Movement")
else:
    unreal.log("WuwaWalkRun: INSPECTION ONLY; no assets changed")
