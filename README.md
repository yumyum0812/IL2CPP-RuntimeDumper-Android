## Description
Runtime IL2CPP dumper for Android

## Disclaimer
This project is used for educational purposed only,
Please use this project at your own risk. We accept no responsibility for any damages.

## Features
- Dump classes, fields, properties, methods
- Dump enum values
- Dump generics of classes, methods
- Dump custom attributes of classes, methods, fields, properties, parameters
- Dump method RVAs, offsets
- Dump instance field offsets
- Dump const field values
- These are generated to `/storage/emulated/0/Android/data/<PACKAGE>/files/RuntimeDumper/Dump/dump.cs`

## Usage
### By injection
Build and load libdumper.so in your game by library injector or other tools.
### By Zygisk module
Build or download Zygisk module from release and install.

## Configuration
### Targeting
You can add/remove target game packages by KernelSU Web UI. if you using classic magisk, install [KsuWebUI Standalone](https://github.com/adivenxnataly/KsuWebUI)
### Adjusing
When the targeted game launches first time after targeted, the configuration file `/storage/emulated/0/Android/data/<PACKAGE>/files/RuntimeDumper/dumper_config.json` will be generated.
The configuration file has following keys.
| Name | Type | Description |
| --- | --- | --- |
| no_dump | bool | Disables dump operation |
| lib_name | string | Dynamic library name of libil2cpp |
| wait_time | int | Time of wait before dumping in seconds |
| filter_assembly | bool | Filters assemblies to dump by names |
| assembly_filter | [string] | Whitelist of assemblies to dump by names |
| class_blacklist | [string] | Blacklist of classes to skip dumping by names |
| trigger_by_hook | boolean | Hook a il2cpp api function and dump once when the that function was executed |
| split_dump | boolean | Splits dump.cs for each assemblies |
### Mapping symbols
You can add the following map files to map obfuscated symbols if you found. if would you add to like these, please create following file to `/storage/emulated/0/Android/data/<PACKAGE>/files/RuntimeDumper`

#### rva_map.json
- Map of RVA offset and API symbol
##### Example
```json
[
    "il2cpp_xxxx": 123456,
    "il2cpp_yyyy": "0xABC123",
    ...
]
...

```
#### symbol_map.json
- Map of obuscated / shuffled symbol and API symbol
##### Example
```json
[
    "il2cpp_xxxx": "abcdef",
    "il2cpp_yyyy": "yjsnpi",
    ...
]
...
```
### Patches
You can add the following file to patch libil2cpp.so for specified offsets, bytes before dumping.
#### patches.json
##### Example
```json
[
    {
        "offset": "0x893893",
        "bytes": "11 45 14 19"
    },
    {
        "offset": 123456,
        "bytes": "AB CD EF 99"
    },
    ...
]
```

## Output example
```cs
// Namespace: UnityEngine
[NativeClass("Rectf", "template<typename T> class RectT; typedef RectT<float> Rectf;")]
[RequiredByNativeCode(Optional = true, GenerateProxy = true)]
[NativeHeader("Runtime/Math/Rect.h")]
public struct Rect : IEquatable<Rect>, IFormattable {
    private float m_XMin; // 0x0
    private float m_YMin; // 0x4
    private float m_Width; // 0x8
    private float m_Height; // 0xC

    public override Rect zero { get; }
    ...

    // RVA: 0x26771DC Offset: 0x26731DC VA: 0x26771DC
    public override void .ctor(float x, float y, float width, float height) { }
    ...
```
```cs
// Namespace: System.Collections.Generic
[DebuggerTypeProxy(typeof(ICollectionDebugView<>))]
[DefaultMember("Item")]
[DebuggerDisplay("Count = {Count}")]
[Serializable]
public class List<T> : IList<T>, ICollection<T>, IEnumerable<T>, IEnumerable, IList, ICollection, IReadOnlyList<T>, IReadOnlyCollection<T> {
    private const int DefaultCapacity = 4;
    private T[] _items; // -0x1
    ...
```