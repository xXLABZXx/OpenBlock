# OpenBlock API coverage matrix

> Generated 2026-10-14 by `openblox --tests`. Machine-checked: every row was exercised by the engine's own test suite. Never hand-edit this file.

| API member | Status | Note |
|---|---|---|
| **Anchored part does not fall** |||
| Anchored part does not fall | ✅ working |  |
| **BindableEvent Fire passes args** |||
| BindableEvent Fire passes args | ✅ working |  |
| **BindableFunction Invoke calls bound function** |||
| BindableFunction Invoke calls bound function | ✅ working |  |
| **BoolValue default and round-trip** |||
| BoolValue default and round-trip | ✅ working |  |
| **BrickColor invalid name errors** |||
| BrickColor invalid name errors | ✅ working |  |
| **BrickColor** |||
| BrickColor.new name | ✅ working |  |
| BrickColor.new number | ✅ working |  |
| **CFrame Angles rotation** |||
| CFrame Angles rotation | ✅ working |  |
| **CFrame GetComponents** |||
| CFrame GetComponents | ✅ working |  |
| **CFrame add and sub** |||
| CFrame add and sub | ✅ working |  |
| **CFrame equality** |||
| CFrame equality | ✅ working |  |
| **CFrame invalid member errors** |||
| CFrame invalid member errors | ✅ working |  |
| **CFrame inverse** |||
| CFrame inverse | ✅ working |  |
| **CFrame lerp** |||
| CFrame lerp | ✅ working |  |
| **CFrame multiplication** |||
| CFrame multiplication | ✅ working |  |
| **CFrame space transforms** |||
| CFrame space transforms | ✅ working |  |
| **CFrame tostring** |||
| CFrame tostring | ✅ working |  |
| **CFrame** |||
| CFrame.new defaults | ✅ working |  |
| CFrame.new lookAt | ✅ working |  |
| CFrame.new position | ✅ working |  |
| **Camera default CFrame** |||
| Camera default CFrame | ✅ working |  |
| **CanCollide=false passes through** |||
| CanCollide=false passes through | ✅ working |  |
| **ClickDetector MouseClick Connect/Disconnect** |||
| ClickDetector MouseClick Connect/Disconnect | ✅ working |  |
| **ClickDetector MouseClick signal stable across accesses** |||
| ClickDetector MouseClick signal stable across accesses | ✅ working |  |
| **ClickDetector creation** |||
| ClickDetector creation | ✅ working |  |
| **ClickDetector input-driven firing** |||
| ClickDetector input-driven firing | ❌ unsupported | headless v0.1 has no input path |
| **Color3** |||
| Color3.new | ✅ working |  |
| **Connection** |||
| Connection.Connected | ✅ working |  |
| **DataStore IncrementAsync** |||
| DataStore IncrementAsync | ✅ working |  |
| **DataStore RemoveAsync clears** |||
| DataStore RemoveAsync clears | ✅ working |  |
| **DataStore SetAsync GetAsync round trip** |||
| DataStore SetAsync GetAsync round trip | ✅ working |  |
| **DataStore UpdateAsync transforms** |||
| DataStore UpdateAsync transforms | ✅ working |  |
| **DataStore rejects unsupported values** |||
| DataStore rejects unsupported values | ✅ working |  |
| **DataStoreService** |||
| DataStoreService | ❌ unsupported | Tier 2, not in v0.1 |
| **DataStoreService GetDataStore caches** |||
| DataStoreService GetDataStore caches | ✅ working |  |
| **Debris** |||
| Debris | ❌ unsupported | Tier 1, not in v0.1 |
| **Debris destroys item after lifetime** |||
| Debris destroys item after lifetime | ✅ working |  |
| **Enum values** |||
| Enum values | ✅ working |  |
| **GetService** |||
| GetService | ✅ working |  |
| **GetService invalid errors** |||
| GetService invalid errors | ✅ working |  |
| **Humanoid** |||
| Humanoid | ❌ unsupported | Tier 1, not in v0.1 |
| **Humanoid BuildRig** |||
| Humanoid BuildRig | ✅ working |  |
| **Humanoid Jump** |||
| Humanoid Jump | ✅ working |  |
| **Humanoid MoveTo walks** |||
| Humanoid MoveTo walks | ✅ working |  |
| **Humanoid TakeDamage and Died** |||
| Humanoid TakeDamage and Died | ✅ working |  |
| **Humanoid health clamps** |||
| Humanoid health clamps | ✅ working |  |
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
| **IntValue default and round-trip** |||
| IntValue default and round-trip | ✅ working |  |
| **Lighting properties round-trip** |||
| Lighting properties round-trip | ✅ working |  |
| **NumberValue default and round-trip** |||
| NumberValue default and round-trip | ✅ working |  |
| **Part CFrame sync** |||
| Part CFrame sync | ✅ working |  |
| **Part lands on ground** |||
| Part lands on ground | ✅ working |  |
| **Part math property defaults** |||
| Part math property defaults | ✅ working |  |
| **Part properties** |||
| Part properties | ✅ working |  |
| **Part property type mismatch errors** |||
| Part property type mismatch errors | ✅ working |  |
| **Part read-only ClassName** |||
| Part read-only ClassName | ✅ working |  |
| **Part** |||
| Part.Touched fires on overlap | ✅ working |  |
| **Player Character assign** |||
| Player Character assign | ✅ working |  |
| **Players GetPlayerByUserId finds player** |||
| Players GetPlayerByUserId finds player | ✅ working |  |
| **Players GetPlayers lists current players** |||
| Players GetPlayers lists current players | ✅ working |  |
| **Players fires PlayerRemoving on Destroy** |||
| Players fires PlayerRemoving on Destroy | ✅ working |  |
| **Players tracks PlayerAdded** |||
| Players tracks PlayerAdded | ✅ working |  |
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
| **Sound Play and Stop toggle Playing** |||
| Sound Play and Stop toggle Playing | ✅ working |  |
| **StringValue default and round-trip** |||
| StringValue default and round-trip | ✅ working |  |
| **Teams GetTeams lists teams** |||
| Teams GetTeams lists teams | ✅ working |  |
| **Tween Cancel stops the tween** |||
| Tween Cancel stops the tween | ✅ working |  |
| **Tween Pause freezes progress** |||
| Tween Pause freezes progress | ✅ working |  |
| **TweenInfo** |||
| TweenInfo.new validates arguments | ✅ working |  |
| **TweenService** |||
| TweenService | ❌ unsupported | Tier 1, not in v0.1 |
| **TweenService Create and Play moves Position** |||
| TweenService Create and Play moves Position | ✅ working |  |
| **TweenService Create rejects unknown property** |||
| TweenService Create rejects unknown property | ✅ working |  |
| **TweenService tweens Transparency** |||
| TweenService tweens Transparency | ✅ working |  |
| **UDim** |||
| UDim.new | ✅ working |  |
| **UDim2 arithmetic** |||
| UDim2 arithmetic | ✅ working |  |
| **UDim2 invalid member errors** |||
| UDim2 invalid member errors | ✅ working |  |
| **UDim2** |||
| UDim2.fromOffset | ✅ working |  |
| UDim2.fromScale | ✅ working |  |
| UDim2.new | ✅ working |  |
| **Unanchored part falls** |||
| Unanchored part falls | ✅ working |  |
| **Value Changed fires with property name** |||
| Value Changed fires with property name | ✅ working |  |
| **Value classes IsA relationships** |||
| Value classes IsA relationships | ✅ working |  |
| **Value classes reject wrong types** |||
| Value classes reject wrong types | ✅ working |  |
| **ValueBase is not directly creatable** |||
| ValueBase is not directly creatable | ✅ working |  |
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
| **Velocity integrates** |||
| Velocity integrates | ✅ working |  |
| **Workspace CurrentCamera and Gravity** |||
| Workspace CurrentCamera and Gravity | ✅ working |  |
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
| **touchCheck scales to 2000 parts** |||
| touchCheck scales to 2000 parts | ✅ working |  |
| **workspace global** |||
| workspace global | ✅ working |  |

Summary: 121 working, 1 partial, 8 unsupported, 0 failed.
