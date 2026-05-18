<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">unreal-mcp · fork</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Beta-yellow)](#)

</div>

This is a **fork** of [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) that
brings the plugin forward to UE 5.6 / 5.7 and substantially extends what the
agent can do inside the editor — most importantly, a recursive property setter
that handles nested structs, arrays of structs, asset references, soft refs,
text / name, and live actor lookups, plus first-class data-asset authoring and
in-editor Live Coding triggered from MCP.

See [CHANGELOG.md](./CHANGELOG.md) for the full delta against upstream.

## ⚠️ Status

Beta. Built daily against UE 5.6 / 5.7. The upstream "experimental" warning
still applies to the surface area inherited from the original project — APIs
will keep moving as agent-driven workflows mature. Don't ship a production
project against a pinned version without a known-good commit hash.

## 🌟 What you can drive from MCP

| Category | Capabilities |
|----------|--------------|
| **Actor management** | Spawn / delete actors (engine types directly, custom C++ types via Blueprint wrap), set transforms, query properties, list and find by name |
| **Property editing** | Read / write properties on actors **and** on data assets. Setter handles bool, int, float, double, int64, FName, FString, FText, enum (byte + class), object refs (asset path or live actor name), soft object refs, **structs (recursive)**, **arrays (incl. arrays of structs)** |
| **Blueprint development** | Create Blueprint classes (with proper parent-class lookup that finds project-defined parents, not just `/Script/Engine`), add and configure components, set static-mesh / physics / **collision profile** / `bCanEverAffectNavigation`, compile (auto-saved), spawn instances |
| **Blueprint node graph** | Event nodes, function-call nodes, variable get / set, connections, self and component references, find nodes |
| **Data assets** | Create new `UDataAsset` subclasses, set any UPROPERTY on them (using the same setter as actors), auto-save, fire `PostEditChange` so the asset's side effects run |
| **Editor control** | Focus viewport on actors or locations, take screenshots |
| **Build loop** | Trigger Live Coding compile from MCP so plugin / game-module code changes hot-reload without leaving the editor |
| **UMG widgets** | Create widget blueprints, add text blocks / buttons, bind events, add to viewport |

All callable via natural language through any MCP client.

## 🧩 Components

### Plugin (`MCPGameProject/Plugins/UnrealMCP`)
- Native TCP server on `127.0.0.1:55557`
- Editor-only module — does not link into shipping builds
- Marshals all command handlers onto the game thread

### Sample Project (`MCPGameProject`)
- Empty starter project with the plugin already enabled
- Drop-in baseline for testing — copy the plugin folder into your own project to use it there

### Python MCP server (`Python/unreal_mcp_server.py`)
- FastMCP-based, registers tool modules from `tools/`
- Maintains the TCP socket to the editor
- 180-second per-request timeout (was 5 s upstream — needed for cold class loads)

## 📂 Directory Structure

- **`MCPGameProject/`** — example UE project with the plugin pre-configured
  - **`Plugins/UnrealMCP/`** — plugin source
    - **`Source/UnrealMCP/`** — C++ source
    - **`UnrealMCP.uplugin`** — plugin descriptor
- **`Python/`** — Python MCP server + tool modules
  - **`tools/`** — tool registrations (actor, blueprint, node, editor, project, UMG)
  - **`scripts/`** — example helper scripts
- **`Docs/`** — long-form docs
- **`CHANGELOG.md`** — version history (fork-specific changes are listed here)

## 🚀 Quick Start

### Prerequisites

- **Unreal Engine 5.6 or newer** (5.5 will not compile — see CHANGELOG)
- **Python 3.12+**
- **`uv`** (Python package runner) — install from <https://astral.sh/uv>
- An MCP client (Claude Code, Claude Desktop, Cursor, Windsurf, etc.)

### Option A — use the sample project

`MCPGameProject` is a Blank Project with the plugin already added.

1. Right-click `.uproject` → **Generate Visual Studio project files**
2. Open the `.sln`, choose **Development Editor / Win64**, build
3. Open the project in the editor

### Option B — add the plugin to your own project

1. Copy `MCPGameProject/Plugins/UnrealMCP` into your project's `Plugins/` folder
2. Right-click `.uproject` → **Generate Visual Studio project files**
3. Build with **Development Editor / Win64** (or your platform target)
4. Open in editor — **Edit → Plugins**, find **UnrealMCP** under *Editor*, verify it's enabled, restart if prompted

The plugin starts its TCP listener automatically when the editor loads.

### Python server setup

```sh
# from the repo root
cd Python
uv sync          # installs the FastMCP + helper deps the server uses
```

See [`Python/README.md`](./Python/README.md) for environment options.

### MCP client configuration

Use this JSON for any FastMCP-compatible client:

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<absolute/path/to/Python>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

An example lives in `mcp.json` at the repo root.

| MCP Client | Configuration File Location |
|---|---|
| Claude Code (CLI) | Add via `claude mcp add unreal-mcp -- uv --directory <PYTHON_DIR> run unreal_mcp_server.py` |
| Claude Desktop | `~/.config/claude-desktop/mcp.json` — on Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` — project root |
| Windsurf | `~/.config/windsurf/mcp.json` — on Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

### Verify the connection

1. Open the editor with the plugin loaded — confirm `MCPServer: Listening on 127.0.0.1:55557` in the Output Log
2. Start your MCP client
3. Ask the agent to *"list all actors in the current level"* — you should get a JSON dump of actors

## 🛠 Common workflows

### Spawn a project-defined C++ actor

The plugin's `spawn_actor` only supports engine-side types (StaticMeshActor,
PointLight, etc). For project-defined C++ classes (your characters, your AI
controllers, your custom actors), wrap them in a Blueprint first:

```
create_blueprint(name="BP_MyActor", parent_class="MyActor")
compile_blueprint("BP_MyActor")             # autosaves to /Game/Blueprints/
spawn_blueprint_actor(blueprint_name="BP_MyActor", actor_name="MyActor_1", location=[0,0,100])
```

Parent-class lookup falls back to a generic `FindFirstObject<UClass>` search,
so project-module classes from `/Script/YourGame.YourClass` resolve without
explicit paths.

### Edit a nested struct or array-of-structs in one call

The property setter accepts JSON for any UPROPERTY-typed field. Pass nested
objects for structs and arrays for `TArray<FStruct>`:

```python
set_actor_property(
    name="MySpawner_1",
    property_name="Spawns",
    property_value=[
        {
            "ActorClass":  "/Game/Blueprints/BP_MyActor.BP_MyActor_C",
            "DisplayName": "Alice",
            "SpawnTransform": {
                "Translation": {"X": 0, "Y": 0, "Z": 200},
                "Rotation":    {"X": 0, "Y": 0, "Z": 0, "W": 1},
                "Scale3D":     {"X": 1, "Y": 1, "Z": 1}
            },
            "Schedule": "/Game/Data/DA_Schedule_A.DA_Schedule_A",
            "HomeActor": "POI_Home_01"   // live-actor lookup by name
        }
    ]
)
```

Object reference fields accept either:
- An asset path (`/Game/...`) — loaded via `StaticLoadObject`
- A live actor name — looked up in the editor world via `TActorIterator`

### Author a data asset

```python
create_data_asset(name="DA_MyConfig", class_name="MyConfigDataAsset", package_path="/Game/Data/")
set_data_asset_property(
    asset_path="/Game/Data/DA_MyConfig.DA_MyConfig",
    property_name="Entries",
    property_value=[...]
)
```

`set_data_asset_property` fires `PostEditChange()` after every write so any
side effects (e.g. arrays that re-sort or caches that rebuild) run as if the
user had edited the asset in the editor.

### Toggle collision / nav on a Blueprint component

```python
set_collision_profile(
    blueprint_name="BP_MyCharacter",
    component_name="CosmeticMesh",
    profile_name="NoCollision",
    can_ever_affect_navigation=False
)
```

### Iterate on plugin code without leaving the editor

Patch C++ in `Plugins/UnrealMCP/Source/` then:

```python
trigger_live_coding()
```

Equivalent to pressing **Ctrl+Alt+F11** in the editor. Asynchronous — watch
the Live Coding console for `Patching complete!` before invoking the
newly-patched commands. Python tool registrations still need a fresh MCP
server session.

## 🔧 Known issues (carried from upstream)

These are non-blocking but flagged for future cleanup — see CHANGELOG for
detail:

- `Json.h` is included as a monolithic header in several files (~10 IWYU
  warnings)
- `HandleTakeScreenshot` uses the deprecated
  `FImageUtils::CompressImageArray` (still compiles in 5.7, will break in a
  future engine version)

## 📜 License

MIT, inherited from the upstream project.

## 🙏 Credits

Upstream: <https://github.com/chongdashu/unreal-mcp> — original plugin and
the Python MCP scaffolding.

This fork adds UE 5.6 / 5.7 compatibility, the recursive property setter,
data-asset authoring, collision/nav helpers, MCP-driven Live Coding, and the
auto-save / parent-class-lookup improvements documented above.
