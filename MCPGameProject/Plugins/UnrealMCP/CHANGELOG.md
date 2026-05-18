# Changelog

All notable changes to this fork of UnrealMCP are recorded here.
Format roughly follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.1.0] — Fork release

This is the first release of the fork. It tracks the upstream plugin but adds
substantial new capability and brings the codebase forward to Unreal Engine
5.6 / 5.7. **Minimum Engine version is now UE 5.6.**

### Compatibility

- **UE 5.6+** is now required. The upstream `ANY_PACKAGE` deprecation requires
  the newer `FindFirstObject<T>` API; the plugin no longer compiles against
  UE 5.5 or earlier.
- Verified to build cleanly under **UE 5.7** via `RunUAT BuildPlugin` against a
  vanilla HostProject (no game module required).
- Plugin still ships as **Editor-only** (`Type: Editor` in the .uplugin).

### Added — new MCP tools

- **`set_collision_profile`** — set a Blueprint component's
  `CollisionProfileName` and/or `bCanEverAffectNavigation` flag. Useful for
  flipping cosmetic meshes to `NoCollision` so they stop blocking the navmesh.
- **`trigger_live_coding`** — fires `LiveCoding.Compile` from MCP, lets
  contributors patch plugin code and reload without leaving the editor.
- **`create_data_asset`** — factory for `UDataAsset` subclasses; resolves the
  asset class via Engine / Game / `FindFirstObject` fallback, creates the
  package, registers with the asset registry, and saves to disk.
- **`set_data_asset_property`** — load an asset, run the generalized property
  setter, fire `PostEditChange()` so the asset's side effects (re-sorting,
  cache rebuilds) run, then save.

### Added — property setter capabilities

`SetObjectProperty` (and the new raw-address recursive helper
`SetPropertyValueAtAddress`) now handles:

- `FNameProperty` — strings deserialized to `FName`.
- `FTextProperty` — strings deserialized via `FText::FromString`.
- `FDoubleProperty` — required for UE 5.x `FVector` components (FVector now
  uses doubles).
- `FInt64Property`.
- `FObjectPropertyBase` — resolves an asset path (`/Game/...`) via
  `StaticLoadObject` or, if no path prefix, looks the value up as a live actor
  name in the editor world via `TActorIterator`.
- `FSoftObjectProperty` — string → `FSoftObjectPath`.
- `FStructProperty` — JSON object → struct field deserialization, recursing
  through every UPROPERTY-declared field.
- `FArrayProperty` — JSON array → `FScriptArrayHelper::Resize` + per-element
  recursion. Works for arrays of primitives **and** arrays of structs.

A JSON-string normalization helper (`NormalizeJsonValue`) auto-parses a
`property_value: str` that starts with `[` or `{` into the corresponding
`FJsonValueArray` / `FJsonValueObject`, so callers can pass JSON inline as a
string when the MCP wire format prefers strings.

### Added — auto-save on mutation

- `HandleCreateBlueprint` — calls `UEditorAssetLibrary::SaveAsset` after the
  factory creates the BP. Without this, downstream tools using
  `FPackageName::DoesPackageExist` couldn't find the unsaved package.
- `HandleCompileBlueprint` — saves the BP package after compile so
  post-compile state survives editor restarts.

### Changed — parent class resolution

`HandleCreateBlueprint` previously searched only `/Script/Engine.X` and
`/Script/Game.X` for the parent class. The fork falls back to
`FindFirstObject<UClass>(...)` if both fail, which makes project-defined
parent classes (e.g. `/Script/MyProjectModule.MyCharacterBase`) resolvable
without hardcoding paths. Result is validated to be an `AActor` descendant.

### Changed — Python side

- Socket timeout bumped from **5 s to 180 s** (`unreal_mcp_server.py`). Cold
  Blueprint class loads for project-defined parents (Character chains with
  animation, AI, nav dependencies) routinely exceed 5 s.
- `set_data_asset_property` accepts `Any` for `property_value` and auto-JSON
  encodes `dict` / `list` arguments before sending. Callers can pass native
  Python objects without manual `json.dumps`.

### Fixed — UE 5.6 / 5.7 API drift

- `MCPServerRunnable.cpp`: renamed file-scope `BufferSize` constant to
  `MCPBufferSize` to avoid a symbol shadow against UE 5.7's new local in
  `Containers/StringConv.h`.
- `UnrealMCPBlueprintCommands.cpp`,
  `UnrealMCPBlueprintNodeCommands.cpp`:
  replaced `FindObject<UClass>(ANY_PACKAGE, ...)` with
  `FindFirstObject<UClass>(...)`. `ANY_PACKAGE` was removed in UE 5.6.

### Fixed — internal

- `FIntProperty` branch in `SetObjectProperty` now uses
  `SetPropertyValue(addr, ...)` instead of `SetPropertyValue_InContainer(obj, ...)`.
  Required for the refactored raw-address recursive path.
- Header-tool error: removed `UFUNCTION` decorators from a few accessors that
  returned `const TArray<TObjectPtr<...>>&` (forbidden in UFUNCTION
  signatures under UE 5.7).

### Known issues / future cleanup (carried from upstream)

These pre-existing warnings still surface; they are not blocking but should
be addressed before a 1.2 release:

- `Json.h` is included as a monolithic header in ~10 files; UE recommends
  explicit IWYU includes (`Dom/JsonObject.h`, `Serialization/JsonReader.h`,
  etc.).
- `HandleTakeScreenshot` calls the deprecated
  `FImageUtils::CompressImageArray`; migrate to `PNGCompressImageArray` or
  `ThumbnailCompressImageArray` before it stops compiling.

### Notes for contributors / users of the fork

- The `trigger_live_coding` tool dramatically tightens the iteration loop on
  plugin development. Patch C++ → call `trigger_live_coding` → next MCP call
  uses the patched code. Python tool registrations still require a fresh MCP
  server (i.e. fresh Claude / Cursor session).
- `set_actor_property` and `set_data_asset_property` both pass `property_value`
  through the same generalized type ladder, so anything documented in the
  "property setter capabilities" section above works for both actor instances
  and data assets.
- See the README for the standard install path (plugin folder + Python
  `uv` server + MCP client registration).
