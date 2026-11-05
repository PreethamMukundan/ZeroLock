"""Creates or updates the item assets from items.json. Run build_items.py first.

Editor console:  py "E:/Repo/ZeroLock/ZeroLock/Tools/ItemImporter/ue_import_items.py"
Editor closed:   item_import.bat runs it headlessly.

Assets go to /Game/Data/Items/ZL, keyed by source ID, so reruns update in place and keep references.
"""
import json
import os
import re
import struct
import zlib

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
ICON_DIR = os.path.join(ROOT, 'Saved', 'ItemImporter', 'Icons')
BASE = '/Game/Data/Items/ZL'
INPUT_ACTIONS = '/Game/ThirdPerson/Input/Actions'
MAPPING_CONTEXT = '/Game/ThirdPerson/Input/IMC_Default'
# C is Ability 2 and F is melee.
ITEM_KEYS = ('Z', 'X', 'V', 'G')
SHOP_WIDGET = '/Game/ZERO/UI/BP/Widgets/ItemShop/WBP_ItemShopBase'
ICON_SIZE = 128
TYPE_COLORS = {'Weapon': (214, 142, 38), 'Spirit': (150, 84, 214), 'Vitality': (66, 176, 92)}


def log(message):
    unreal.log_warning('ITEMIMPORT ' + message)


def enum_member(enum, name):
    """EZL_ItemStat::MaxHealth -> MAX_HEALTH."""
    return getattr(enum, re.sub(r'(?<=[a-z0-9])(?=[A-Z])', '_', name).upper())


def write_png(path, pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

    rows = b''.join(b'\0' + bytes(c for pixel in row for c in pixel) for row in pixels)
    header = struct.pack('>IIBBBBB', ICON_SIZE, ICON_SIZE, 8, 2, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def placeholder_pixels(color, tier):
    """A flat type-coloured tile with one pip per tier along the bottom."""
    dark = tuple(c // 3 for c in color)
    pixels = [[dark if x < 6 or y < 6 or x >= ICON_SIZE - 6 or y >= ICON_SIZE - 6 else color for x in range(ICON_SIZE)] for y in range(ICON_SIZE)]
    pip, gap = 14, 8
    start = (ICON_SIZE - tier * pip - (tier - 1) * gap) // 2
    for index in range(tier):
        left = start + index * (pip + gap)
        for y in range(ICON_SIZE - 34, ICON_SIZE - 34 + pip):
            for x in range(left, left + pip):
                pixels[y][x] = (240, 240, 240)
    return pixels


def import_icons(needed):
    os.makedirs(ICON_DIR, exist_ok=True)
    tasks = []
    for item_type, tier in sorted(needed):
        name = f'T_Item_{item_type}_{tier}'
        path = os.path.join(ICON_DIR, name + '.png')
        write_png(path, placeholder_pixels(TYPE_COLORS[item_type], tier))
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', path)
        task.set_editor_property('destination_path', BASE + '/Icons')
        task.set_editor_property('destination_name', name)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', False)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    icons = {}
    for item_type, tier in needed:
        texture = unreal.load_asset(f'{BASE}/Icons/T_Item_{item_type}_{tier}')
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('never_stream', True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
        icons[(item_type, tier)] = texture
    return icons


def asset_path(item):
    return f"{BASE}/{item['type']}/Tier{item['tier']}/DA_{item['id']}"


def load_or_create_item(item):
    path = asset_path(item)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.Zero_Item_data)
    package, name = path.rsplit('/', 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, unreal.Zero_Item_data, factory)


def make_stats(entries):
    stats = []
    for entry in entries:
        value = unreal.ZL_ItemStatValue()
        value.set_editor_property('stat', enum_member(unreal.ZL_ItemStat, entry['stat']))
        value.set_editor_property('value', entry['value'])
        stats.append(value)
    return stats


def make_active(data):
    active = unreal.ZL_ActiveItem()
    if not data:
        return active
    active.set_editor_property('target', enum_member(unreal.ZL_ActiveItemTarget, data['target']))
    active.set_editor_property('area_team', enum_member(unreal.ZL_ActiveItemTeam, data['area_team']))
    for field in ('cooldown', 'duration', 'range', 'radius', 'damage', 'heal', 'barrier', 'barrier_duration'):
        active.set_editor_property(field, data[field])
    active.set_editor_property('timed_stats', make_stats(data['timed_stats']))
    active.set_editor_property('statuses', [enum_member(unreal.ZL_ItemStatus, status) for status in data['statuses']])
    active.set_editor_property('status_duration', data['status_duration'])
    active.set_editor_property('cleanse', data['cleanse'])
    for field in ('delay', 'heal_max_health_percent', 'dot_damage_per_second', 'dot_max_health_percent', 'dot_duration', 'zone_duration',
                  'zone_damage_per_second', 'dash_distance', 'dash_to_target', 'launch_speed', 'pull_to_point'):
        active.set_editor_property(field, data[field])
    active.set_editor_property('caster_stats', make_stats(data['caster_stats']))
    active.set_editor_property('caster_statuses', [enum_member(unreal.ZL_ItemStatus, status) for status in data['caster_statuses']])
    return active


def make_proc(data):
    proc = unreal.ZL_ItemProc()
    for field, value in data.items():
        if field == 'trigger':
            value = enum_member(unreal.ZL_ProcTrigger, value)
        elif field == 'status':
            value = enum_member(unreal.ZL_ItemStatus, value)
        elif field == 'area_team':
            value = enum_member(unreal.ZL_ActiveItemTeam, value)
        elif field in ('self_stats', 'target_stats'):
            value = make_stats(value)
        proc.set_editor_property(field, value)
    return proc


def setup_item_keys():
    """IA_Item1-4 and their IMC_Default mappings. Safe to rerun: existing actions and mappings are left alone."""
    context = unreal.load_asset(MAPPING_CONTEXT)
    mapped = {mapping.get_editor_property('action') for mapping in context.get_editor_property('default_key_mappings').get_editor_property('mappings')}

    for index, key_name in enumerate(ITEM_KEYS, start=1):
        name = f'IA_Item{index}'
        path = f'{INPUT_ACTIONS}/{name}'
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            action = unreal.load_asset(path)
        else:
            action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, INPUT_ACTIONS, unreal.InputAction, unreal.InputAction_Factory())
            settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=action)
            settings.set_editor_property('name', name)
            settings.set_editor_property('display_name', unreal.Text(f'Item {index}'))
            settings.set_editor_property('display_category', unreal.Text('Items'))
            action.set_editor_property('player_mappable_key_settings', settings)
            unreal.EditorAssetLibrary.save_loaded_asset(action, False)

        if action not in mapped:
            key = unreal.Key()
            key.set_editor_property('key_name', key_name)
            context.map_key(action, key)
    unreal.EditorAssetLibrary.save_loaded_asset(context, False)


def fill_item(asset, item, icon):
    asset.set_editor_property('item_id', item['id'])
    asset.set_editor_property('item_name', item['name'])
    asset.set_editor_property('cost', item['cost'])
    asset.set_editor_property('description', item['description'])
    asset.set_editor_property('item_type', enum_member(unreal.ZeroLockItemType, item['type']))
    asset.set_editor_property('item_tier_type', getattr(unreal.ZeroLockItemTier, f"TIER{item['tier']}"))
    asset.set_editor_property('icon', icon)
    asset.set_editor_property('stats', make_stats(item['stats']))
    asset.set_editor_property('active', make_active(item.get('active')))
    asset.set_editor_property('procs', [make_proc(proc) for proc in item['procs']])
    asset.set_editor_property('imbue', item['imbue'])
    asset.set_editor_property('imbue_stats', make_stats(item['imbue_stats']))


def build_table(items):
    path = f'{BASE}/DT_ZL_Items'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        table = unreal.load_asset(path)
    else:
        factory = unreal.DataTableFactory()
        factory.set_editor_property('struct', unreal.ItemTableRow.static_struct())
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DT_ZL_Items', BASE, unreal.DataTable, factory)

    rows = []
    for item in items:
        if item['in_shop']:
            package = asset_path(item)
            rows.append({'Name': item['id'], 'ItemAsset': f"{package}.{package.rsplit('/', 1)[1]}"})
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        raise RuntimeError('Could not fill the item table')
    unreal.EditorAssetLibrary.save_loaded_asset(table, False)
    return table, len(rows)


def point_shop_at(table):
    blueprint = unreal.load_asset(SHOP_WIDGET)
    shop = unreal.get_default_object(blueprint.generated_class())
    if shop.get_editor_property('item_table') != table:
        shop.set_editor_property('item_table', table)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)


def main():
    with open(os.path.join(HERE, 'items.json'), encoding='utf-8') as f:
        items = json.load(f)

    setup_item_keys()
    icons = import_icons({(item['type'], item['tier']) for item in items})

    assets = {}
    for item in items:
        asset = load_or_create_item(item)
        fill_item(asset, item, icons[(item['type'], item['tier'])])
        assets[item['id']] = asset

    # Second pass: every asset has to exist before items can point at each other.
    for item in items:
        asset = assets[item['id']]
        asset.set_editor_property('next_items_to_upgrade', [assets[target] for target in item['upgrades_to']])
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)

    table, shop_rows = build_table(items)
    point_shop_at(table)
    log(f'done: {len(items)} item assets, {shop_rows} in the shop')


main()
