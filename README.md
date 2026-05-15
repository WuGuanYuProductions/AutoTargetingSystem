🎯 目标锁定组件 (Targeting Component) 配置说明文档
1. 概述
这是一个类似于《塞尔达传说》或《黑暗之魂》的“镜头锁定/目标追踪”组件。它可以附加在玩家角色上，实现自动搜索范围内最合适的敌人、平滑锁定镜头、动态调整镜头距离与高度，以及在多个目标间切换的功能。

2. 前置准备 (非常重要)
为了让组件能够识别敌人，所有可以被锁定的敌人必须在类设置（Class Settings）中继承并实现 TargetableInterface（可锁定目标接口）。
敌人蓝图需要实现以下三个接口事件/函数：

GetCurrentHealth: 返回敌人当前生命值（生命值 ≤ 0 时组件会自动取消锁定或切换目标）。
OnTargeted: 当被玩家锁定时触发（策划可在此处播放被锁定的UI特效或音效）。
OnUntargeted: 当玩家取消锁定该目标时触发（用于隐藏锁定UI等）。
3. 面板配置参数 (Details Panel)
选中玩家身上的 TargetingComponent，在细节面板中可以调整以下参数：

基础设置 (Targeting | Config)
Targeting Radius (锁定半径): float。玩家可以搜索和锁定敌人的最大有效距离。超出此距离目标会丢失。
Target Object Types (目标物体类型): Array。组件只会扫描这些碰撞通道的物体（建议专门为敌人建立一个 Object Channel，例如 "Enemy"）。
锁定权重与打分系统 (Targeting | Config | Scoring)
组件内部使用“分数越低，优先级越高”的算法来寻找最佳目标。公式大致为：(血量 * 血量权重) + (距离 * 距离权重)。

Health Weight (生命值权重): float。影响根据血量寻找目标的倾向。如果想优先锁定残血敌人，可适当提高此权重。
Distance Weight (距离权重): float。影响根据距离寻找目标的倾向。数值越大，越倾向于锁定离玩家最近的敌人。
镜头表现 (Targeting | Config | Camera)
Lock On Offset (锁定偏移): FVector。镜头看向敌人的具体位置偏移。通常设为 (0, 0, 50) 等数值，让镜头看向敌人的胸口或头部，而不是脚底。
Camera Interp Speed (镜头插值速度): float。镜头旋转和追踪的平滑度。数值越大镜头跟随越快、越硬；数值越小越平滑、但可能会有延迟感。
Min / Max Camera Distance (最小/最大镜头距离): float。锁定目标后，弹簧臂（SpringArm）的长度范围。距离敌人近时镜头拉近，距离远时镜头拉远。
Min / Max Camera Height (最小/最大镜头高度): float。锁定目标后，镜头的 Z 轴高度范围。用于在打巨型BOSS或贴身肉搏时动态抬高/压低镜头。
Min / Max Camera Pitch (最小/最大镜头俯仰角): float。限制镜头上下旋转的角度（如 -60 到 45），防止镜头穿模或视角反转。
4. 蓝图可用节点 (Blueprint API)
常用操作函数 (Functions)
StartTargeting (返回 Bool): 尝试开启锁定。如果范围内有合法目标则锁定并返回 True，否则返回 False。
StopTargeting: 手动取消当前锁定，并将镜头平滑恢复到锁定前的原始状态。
SwitchToPrevEnemy / SwitchToNextEnemy: 按照距离在屏幕内的敌人之间进行左/右（上/下）目标切换。
GetCurrentLockedEnemy (返回 Actor): 获取当前正在被锁定的敌人（如果没有锁定则返回空）。
CameraLockToEnemy: 必须在开始时调用！ 传入玩家身上的 Camera Component。组件需要接管这个镜头和它连接的弹簧臂（SpringArm）才能生效。
ForceResetCamera: （安全备用机制）如果发现取消锁定后镜头卡住了，调用此函数可以瞬间强制复位镜头。
事件 (Events)
OnTargetStopped: 当锁定状态结束（主动取消、敌人死亡、敌人超出距离）时触发。策划可在此绑定事件以重置玩家状态或关闭锁定UI。
5. 核心底层逻辑提示
屏幕内外优先级：组件会绝对优先锁定当前在屏幕视野内的敌人。只有当视野内没有任何敌人时，才会去锁定屏幕背面（视野外）的敌人。
动态索敌：在锁定状态下，如果出现了一个“更近/更适合”的敌人，系统会自动切换过去。但在玩家手动切换目标后，动态索敌会暂停，直到下一次重新开启锁定。


🎯 Targeting Component Configuration Documentation for Designers
1. Overview
This is a "Lock-On/Target Tracking" component similar to the systems found in Zelda or Dark Souls. Attached to a player character, it automatically scans for the best enemy in range, smoothly controls the camera to track them, dynamically adjusts camera distance and height, and allows switching between multiple targets.

2. Prerequisites & Setup (Crucial)
For the component to recognize an actor as an enemy, all lock-able enemies must implement the TargetableInterface in their Class Settings.
The enemy Blueprint must implement the following interface functions/events:

GetCurrentHealth: Must return the enemy's current health. (If Health ≤ 0, the component automatically drops the target).
OnTargeted: Triggered when the player locks onto this enemy. (Designers can use this to play UI lock-on animations or sounds).
OnUntargeted: Triggered when the lock-on is dropped or switched away from this enemy. (Used to hide UI, etc.).
3. Details Panel Configuration (Variables)
When selecting the TargetingComponent on your player character, you can adjust the following parameters in the Details panel:

Basic Settings (Targeting | Config)
Targeting Radius: float. The maximum effective distance for searching and keeping a target. If the target moves beyond this radius, the lock is broken.
Target Object Types: Array. The collision channels the scanner looks for. (It is highly recommended to create a specific Object Channel like "Enemy" for this).
Scoring System & Weights (Targeting | Config | Scoring)
The component uses a "Lower Cost = Higher Priority" algorithm to find the best target. The rough formula is (Health * HealthWeight) + (Distance * DistanceWeight).

Health Weight: float. How much remaining health affects targeting priority. Increase this if you want the system to prioritize enemies with lower health (finishing off weak enemies).
Distance Weight: float. How much distance affects priority. A higher value strongly forces the system to pick the closest enemy to the player.
Camera Behavior (Targeting | Config | Camera)
Lock On Offset: FVector. Where the camera looks relative to the enemy's pivot point. Usually set to something like (0, 0, 50) so the camera looks at the chest/head rather than their feet.
Camera Interp Speed: float. The smoothness/speed of camera rotation and movement. Higher values make tracking tighter and faster; lower values make it smoother but potentially laggy.
Min / Max Camera Distance: float. The dynamic bounds for the SpringArm length. The camera will pull back when the enemy is far and push in when they are close.
Min / Max Camera Height: float. The dynamic bounds for the SpringArm Z-offset. Useful for tilting the camera up when fighting giant bosses or down when fighting close-range.
Min / Max Camera Pitch: float. Hard limits for how far the camera can rotate up or down (e.g., -60 to 45) to prevent clipping into the floor or flipping upside down.
4. Blueprint API (Nodes)
Action Functions
StartTargeting (Returns Bool): Attempts to lock on. Returns True if a valid target is found and locked, False otherwise.
StopTargeting: Manually breaks the lock-on and smoothly interpolates the camera back to its original default state.
SwitchToPrevEnemy / SwitchToNextEnemy: Cycles through valid targets based on distance.
GetCurrentLockedEnemy (Returns Actor): Gets the reference to the currently locked enemy (returns Invalid/Null if not targeting).
CameraLockToEnemy: Must be called at BeginPlay! Pass the Player's Camera Component into this. The component needs to hijack this camera and its parent SpringArm for the logic to work.
ForceResetCamera: (Failsafe mechanism). If you find the camera stuck in a weird state after untargeting, call this to instantly snap the camera back to default settings.
Events
OnTargetStopped: A delegate event fired when targeting completely ends (due to manual cancellation, enemy death, or out-of-range). Designers can bind to this to reset player stances or close main UI widgets.
5. Core Logic Rules (Under the Hood)
On-Screen Priority: The system heavily prioritizes enemies currently visible on the screen viewport. It will only attempt to target enemies behind the player (off-screen) if the screen is completely empty of valid enemies.
Dynamic Re-evaluating: While locked on, the system constantly checks if a better target appears. However, if the player manually switches targets, this dynamic auto-switching is paused until the lock-on is completely reset to respect player choice.
