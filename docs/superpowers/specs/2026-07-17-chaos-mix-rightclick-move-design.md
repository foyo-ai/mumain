# Right-click move items in the Chaos Machine (mix) window

**Date:** 2026-07-17
**Component:** `mu-client` (C++ game client UI)
**Server change required:** None

## Problem

When a combine/craft window (Chaos Machine at Noria, Dark Raven / Dark Horse
creation, sockets, seeds, cards, etc.) is open, the only way to put an item into
the combine grid or take it back out is drag-and-drop, one item at a time. This
is slow and tedious.

## Goal

Let the player **right-click** an item to move it:

- From the **player inventory** into the **combine grid** (mix box).
- From the **combine grid** back into the **player inventory**.

This mirrors the existing right-click auto-move that already works for the
**Warehouse** (`INTERFACE_STORAGE`).

## Key facts established during investigation

- All craft/combine variants share a single window class: `CNewUIMixInventory`.
  Its grid is a `CNewUIInventoryCtrl` of storage type `STORAGE_TYPE::CHAOS_MIX`
  (8×4). Adding the behavior to this one window covers every combine type.
- Both drag and right-click moves use the same network request,
  `SendRequestEquipmentItem(srcType, srcIndex, item, dstType, dstIndex)`. The
  server treats them identically, so **no protocol/server change is needed**.
- The storage type value the server expects for the mix grid is
  `g_MixRecipeMgr.GetMixInventoryEquipmentIndex()` (not the raw `CHAOS_MIX`
  enum). Confirmed via `CNewUIPickedItem::GetSourceStorageType()`.
- Index helpers on `CNewUIInventoryCtrl` already return offset-correct lineal
  positions: `GetIndexByItem(item)` and `FindEmptySlot(cx, cy)`. The player
  inventory exposes `FindEmptySlotIncludingExtensions(ITEM*)`.
- Right-click input (`SEASON3B::IsPress(VK_RBUTTON)`) is **edge-triggered**
  (true for exactly one frame per physical press), so no continuous/same-frame
  double-send is possible.

## Existing pattern being mirrored

Warehouse right-click auto-move (`CNewUIStorageInventory`):

- Inventory → storage: `ProcessMyInvenItemAutoMove(sourceCtrl)`, invoked by the
  action controller's `HandleStorageAutoMove` when the mouse is over the player
  inventory and `INTERFACE_STORAGE` is visible.
- Storage → inventory: `ProcessStorageItemAutoMove()`, invoked by the storage
  window's own mouse handling when the mouse is over the storage grid.

The mix window will follow the same split.

## Design

Two hook points, because the two directions reach the input pipeline through
different windows.

### Direction A — inventory → mix grid

Right-click on an item in the player inventory routes through
`CNewUIMyInventory::InventoryProcess` →
`CNewUIInventoryActionController::HandleRightClick`.

Today that method **explicitly excludes** the mix window
(`!IsVisible(INTERFACE_MIXINVENTORY)`), so nothing happens.

Change:

1. In `HandleRightClick`, add a branch (next to the existing storage branch):

   ```cpp
   if (g_pNewUISystem->IsVisible(INTERFACE_MIXINVENTORY))
       return HandleMixAutoMove(targetControl);
   ```

2. New private method `HandleMixAutoMove(targetControl)` delegates to the mix
   window (mirrors `HandleStorageAutoMove`):

   ```cpp
   return g_pMixInventory->ProcessMyInvenItemAutoMove(targetControl);
   ```

3. New public method `CNewUIMixInventory::ProcessMyInvenItemAutoMove(sourceCtrl)`:
   - Bail if mix state != `MIX_READY`, an item is on the cursor, or ctrl is null.
   - Find item under mouse in `sourceCtrl`.
   - `FindEmptySlot(width, height)` in the mix grid; bail if −1.
   - Delegate to the shared `AutoMovePickAndSend` helper (see below) with
     destination = mix storage type.

### Direction B — mix grid → inventory

Right-click on an item in the mix grid is **not** currently routed to the action
controller (the grid ctrl only handles left-click itself). The mix window's
`UpdateMouseEvent` already swallows right-clicks over its area.

Change:

1. New private method `CNewUIMixInventory::ProcessMixItemAutoMove()`:
   - Bail if mix state != `MIX_READY`, item on cursor, ctrl null, or mouse not
     over the mix grid rect.
   - Find item under mouse in the mix grid.
   - `g_pMyInventory->FindEmptySlotIncludingExtensions(item)`; bail if −1.
   - Delegate to `AutoMovePickAndSend` with destination = `INVENTORY`.

2. Call it from `UpdateMouseEvent`, inside the existing
   `CheckMouseIn(...) + IsPress(VK_RBUTTON)` block, **before** the click is
   swallowed:

   ```cpp
   if (SEASON3B::IsPress(VK_RBUTTON))
   {
       ProcessMixItemAutoMove();
       MouseRButton = false; MouseRButtonPop = false; MouseRButtonPush = false;
       return false;
   }
   ```

### Shared helper — `AutoMovePickAndSend` (critical)

Both directions route through one static helper. **The item must be *picked* out
of the source grid before the move request is sent** — a naive direct
`SendRequestEquipmentItem` does NOT work:

- The server confirms a move with a single `ItemExtended` (0x24) packet that
  carries only the **destination** storage + slot. The client's
  `ReceiveEquipmentItemExtended` handler adds the item to the destination but has
  no way to remove it from the source — it relies on the source already being
  removed client-side (drag does this at pick time; the warehouse auto-move uses
  a separate `SetItemAutoMove`/`...Success()` state machine).
- Sending directly without picking leaves the item in the source grid and can
  duplicate it client-side until the next inventory resync.

So the helper mirrors the drag path:

```cpp
CNewUIInventoryCtrl::CreatePickedItem(sourceCtrl, item);   // clone onto cursor
sourceCtrl->RemoveItem(item);                              // remove from source grid
picked->HidePickedItem();
SendRequestEquipmentItem(picked->GetSourceStorageType(),   // = INVENTORY or CHAOS_MIX
                         picked->GetSourceLinealPos(),
                         picked->GetItem(), dstType, dstIndex);
// on send failure: CNewUIInventoryCtrl::BackupPickedItem();  // restore, never lose the item
```

The existing response handler then finalizes: `DeletePickedItem()` on success,
`BackupPickedItem()` on failure. This reuses the proven, correct drag machinery
for source removal and rollback.

## Behavior decisions (confirmed with user)

- **Any item** may be moved into the mix grid (no `IsMixSource` filter) — matches
  the Warehouse, maximal flexibility.
- **No free slot** (grid or inventory full) → silently do nothing, exactly like
  the Warehouse.
- Only active while mix state is `MIX_READY` (not during an in-progress
  combine / while the grid is locked).
- If an item is already picked up on the cursor (mid-drag), right-click is
  ignored — drag and right-click never interfere.
- **Double-send:** relies on edge-triggered `IsPress` plus server authority (a
  second request for an item no longer in that slot is rejected server-side). No
  persistent client lock is added; this matches the observable Warehouse UX.

## Files touched

- `src/source/UI/NewUI/Inventory/NewUIInventoryActionController.h` — declare
  `HandleMixAutoMove`.
- `src/source/UI/NewUI/Inventory/NewUIInventoryActionController.cpp` — branch in
  `HandleRightClick`, implement `HandleMixAutoMove`, include
  `NewUIMixInventory.h`.
- `src/source/UI/NewUI/Inventory/NewUIMixInventory.h` — declare
  `ProcessMyInvenItemAutoMove` (public) and `ProcessMixItemAutoMove` (private).
- `src/source/UI/NewUI/Inventory/NewUIMixInventory.cpp` — implement both,
  call `ProcessMixItemAutoMove` from `UpdateMouseEvent`.

## Testing / verification

No C++ unit-test harness exists for the client UI. Verification was done
in-game against the local OpenMU server (testgm @ Noria Chaos Goblin, 180/103):

- ✅ Compiles the `Main` target cleanly (VS dev shell).
- ✅ Right-click inventory jewel → jumps into the combine grid, **and is
  removed from the inventory slot** (no duplicate). Recipe prediction updates.
- ✅ Right-click grid jewel → jumps back to the inventory, **and is removed
  from the grid** (grid returns to "Please upload the assembly items").
- ✅ Moving several items in, then all back out, preserves the exact inventory
  contents — no item loss, no duplication.
- Full-grid / full-inventory → nothing happens (silent), no crash.
- Drag-and-drop still works unchanged.

Note: an earlier direct-send implementation appeared to work for
inventory→grid but silently failed grid→inventory and risked duplication; the
`AutoMovePickAndSend` (pick-first) approach above fixed both. This was only
caught by in-game testing — it compiled and looked correct statically.

## Out of scope

- Server, protocol, and the existing drag-and-drop path (unchanged, kept).
- `IsMixSource` filtering, quantity pickers, "move all matching" shortcuts.
