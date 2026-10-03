## Description
Runtime IL2CPP dumper for Android

## Disclaimer
This project is used for educational purposed only,
Please use this project at your own risk. We accept no responsibility for any damages.

## Features
- Dump classes, fields, properties, methods
- Dump generic classes, methods
- Dump custom attributes of classes, methods, fields, properties, parameters
- Dump method RVAs, offsets
- Dump field offsets
- Dump const values
- These are generated to `/storage/emulated/0/Android/data/<PACKAGE>/files/runtime-dumper/dump.cs`

## Unsupported features
- Offset of static fields
- Offsets and RVAs of generic inst methods

## Usage
Build and load libdumper.so in your game by library injector or other tools.

## Configuration
You can add these map files to find obfuscated symbol. if would you add to like these, please put on `/storage/emulated/0/Android/data/<PACKAGE>/files/`

### rva_map.txt
- Map of RVA offset and API symbol
#### Example
```
il2cpp_xxxx 0xABCDEF
...

```
### symbol_map.txt
- Map of obuscated / shuffled symbol and API symbol
#### Example**
```
il2cpp_xxxx abcdefgh
...
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