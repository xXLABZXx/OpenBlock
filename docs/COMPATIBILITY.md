# OpenBlox compatibility matrix

> Generated 2026-10-14 by `openblox --tests`. Machine-checked: every row was exercised by the test suite. Never hand-edit this file.

| API member | Status | Note |
|---|---|---|
| **CFrame** |||
| CFrame | ❌ unsupported | not implemented in v0.1 |
| **Color3** |||
| Color3.new | ✅ working |  |
| **Connection** |||
| Connection.Connected | ✅ working |  |
| **DataStoreService** |||
| DataStoreService | ❌ unsupported | Tier 2, not in v0.1 |
| **Debris** |||
| Debris | ❌ unsupported | Tier 1, not in v0.1 |
| **GetService** |||
| GetService | ✅ working |  |
| **GetService invalid errors** |||
| GetService invalid errors | ✅ working |  |
| **Humanoid** |||
| Humanoid | ❌ unsupported | Tier 1, not in v0.1 |
| **Instance Changed fires** |||
| Instance Changed fires | ✅ working |  |
| **Instance ChildAdded fires** |||
| Instance ChildAdded fires | ✅ working |  |
| **Instance ChildRemoved fires** |||
| Instance ChildRemoved fires | ✅ working |  |
| **Instance Destroy** |||
| Instance Destroy | ✅ working |  |
| **Instance FindFirstChild** |||
| Instance FindFirstChild | ✅ working |  |
| **Instance GetDescendants** |||
| Instance GetDescendants | ✅ working |  |
| **Instance GetPropertyChangedSignal** |||
| Instance GetPropertyChangedSignal | ✅ working |  |
| **Instance IsA** |||
| Instance IsA | ✅ working |  |
| **Instance WaitForChild** |||
| Instance WaitForChild | ✅ working |  |
| **Instance attributes** |||
| Instance attributes | ✅ working |  |
| **Instance invalid member errors** |||
| Instance invalid member errors | ✅ working |  |
| **Instance parenting** |||
| Instance parenting | ✅ working |  |
| **Instance tags** |||
| Instance tags | ✅ working |  |
| **Instance** |||
| Instance.Clone | ✅ working |  |
| Instance.new Part | ✅ working |  |
| Instance.new invalid class errors | ✅ working |  |
| Instance.new with parent arg | ✅ working |  |
| **Part properties** |||
| Part properties | ✅ working |  |
| **Part property type mismatch errors** |||
| Part property type mismatch errors | ✅ working |  |
| **Part read-only ClassName** |||
| Part read-only ClassName | ✅ working |  |
| **Part** |||
| Part.Touched fires on overlap | ✅ working |  |
| **Players** |||
| Players.PlayerAdded | ❌ unsupported | Tier 1, not in v0.1 |
| **RemoteEvent** |||
| RemoteEvent | ❌ unsupported | Tier 1, not in v0.1 |
| **RunService** |||
| RunService.Heartbeat fires | ✅ working |  |
| RunService.IsServer | ✅ working |  |
| **Signal Connect+Fire via Changed** |||
| Signal Connect+Fire via Changed | ✅ working |  |
| **Signal Disconnect** |||
| Signal Disconnect | ✅ working |  |
| **Signal Once** |||
| Signal Once | ✅ working |  |
| **Signal Wait** |||
| Signal Wait | ✅ working |  |
| **TweenService** |||
| TweenService | ❌ unsupported | Tier 1, not in v0.1 |
| **UDim/UDim2/Enum** |||
| UDim/UDim2/Enum | ❌ unsupported | not implemented in v0.1 |
| **Vector3** |||
| Vector3 | 🟡 partial | Lerp, Cross, Dot, unit etc. not in v0.1 |
| **Vector3 Magnitude** |||
| Vector3 Magnitude | ✅ working |  |
| **Vector3 arithmetic** |||
| Vector3 arithmetic | ✅ working |  |
| **Vector3 equality** |||
| Vector3 equality | ✅ working |  |
| **Vector3 invalid member errors** |||
| Vector3 invalid member errors | ✅ working |  |
| **Vector3** |||
| Vector3.new | ✅ working |  |
| **Workspace** |||
| Workspace.Raycast | ❌ unsupported | Tier 1, not in v0.1 |
| **game global** |||
| game global | ✅ working |  |
| **require ModuleScript** |||
| require ModuleScript | ✅ working |  |
| **require caches result** |||
| require caches result | ✅ working |  |
| **require non-module errors** |||
| require non-module errors | ✅ working |  |
| **script global** |||
| script global | ✅ working |  |
| **task** |||
| task.cancel | ✅ working |  |
| task.defer runs after current | ✅ working |  |
| task.delay | ✅ working |  |
| task.spawn passes args | ✅ working |  |
| task.spawn returns thread | ✅ working |  |
| task.spawn runs | ✅ working |  |
| task.wait yields time | ✅ working |  |
| **workspace global** |||
| workspace global | ✅ working |  |

Summary: 49 working, 1 partial, 9 unsupported, 0 failed.
