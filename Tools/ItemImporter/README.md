# Item importer

Builds the shop items from item data.

```
item_import.bat                 use the cached API response
item_import.bat --refresh       download the item data again
item_import.bat --show-partial  also list items whose effect isn't built yet (their stats still work)
```

| File | Role |
|---|---|
| `names.json` | Display names, keyed by source ID. Edit freely: the tool only reads it. Item names, icons and description text are never taken from the source data. |
| `build_items.py` | API → `items.json` and `Saved/ItemImporter/report.md`. |
| `items.json` | Generated. Don't edit: change `build_items.py` or `names.json`. |
| `ue_import_items.py` | `items.json` → `/Game/Data/Items/ZL` (`DA_<id>`, placeholder icons, `DT_ZL_Items`), and points `WBP_ItemShopBase` at the table. Safe to rerun. |

**Status** in the report: `complete` (all stats and any active work, no other effect), `partial` (some stats work, and it needs an effect or stats that don't exist yet), `unsupported` (none of its stats work yet). Only `complete` items are in the shop by default.

To support a new stat, add it to `EZL_ItemStat` and `UZL_GE_ItemStats`, then to `STATS` in `build_items.py`.

To give an active item its key, add it to `ACTIVE_COMPLETE` or `ACTIVE_PARTIAL` in `build_items.py`, with its cast type in `ACTIVE_TARGETS` if the default is wrong (Self for instant casts, Enemy for aimed ones). The build stops if an active on those lists has nothing to do, or would damage allies. Actives that hit enemies keep only their negative stats.

To give an item a passive effect, add a recipe to `PROC_RECIPES` in `build_items.py` (fields: `PROC_DEFAULTS`, meanings: `FZL_ItemProc`). A string in a recipe is a source property and a missing one stops the build, so a typo can't ship as a 0. List the item in `PROC_PARTIAL` if the recipe leaves anything out. Descriptions are generated from the numbers.

`ue_import_items.py` also creates `IA_Item1-4` and maps them in `IMC_Default` (Z, X, V, G), leaving existing ones alone.
