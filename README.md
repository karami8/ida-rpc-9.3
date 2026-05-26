# RPC For IDA 9.3

IDA-RPC is an IDA Pro plugin that shows the current IDA session status in Discord Rich Presence.

After the plugin is loaded, Discord can display that you are currently analyzing a program in IDA.

## Install

Copy the plugin DLL to the IDA Pro 9.3 plugins directory:

```text
<IDA installation directory>\plugins\
```

Then restart IDA Pro 9.3.

Plugin menu location:

```text
Edit -> Plugins -> IDA RPC
```

Default shortcut:

```text
Ctrl-Alt-R
```

## Build

### Requirements

- Visual Studio 2026 with the C++ desktop development workload
- IDA Pro 9.3 SDK

The IDA SDK can be placed anywhere. The build uses the SDK `src` directory, which should contain:

```text
<IDA SDK>\src\include\ida.hpp
```

### Set the SDK Path

Open Developer PowerShell for VS, or x64 Native Tools Command Prompt for VS.

If you are using Developer PowerShell, set `IDA_SDK_DIR`:

```powershell
$env:IDA_SDK_DIR = "<IDA SDK>\src"
```

If you are using x64 Native Tools Command Prompt:

```bat
set IDA_SDK_DIR=<IDA SDK>\src
```

### Build the Plugin

Run this command from the project root:

```powershell
msbuild .\ida-rpc.sln /p:Configuration=Release64 /p:Platform=x64 /t:Rebuild
```

After a successful build, the plugin DLL will be generated at:

```text
x64\Release64\ida-rpc64.dll
```
