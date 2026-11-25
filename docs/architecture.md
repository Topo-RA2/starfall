# 模块边界与构建

当前按配置、科研模型、引擎效果和展示拆分独立编译单元。游戏扩展仍输出一个 `Starfall.dll`，配置字段、存档格式和每帧执行顺序沿用原有行为。

| 文件 | 责任及状态所有权 |
| --- | --- |
| `src/ConfigText.h` / `ConfigText.cpp` | 共用的字符串规范化、列表和非负整数解析、显示文字解码、配置诊断。不保存配置或游戏状态 |
| `src/Research.h` / `Research.cpp` | 科研配置与运行状态的唯一定义位置；点数、所有权、抽卡、刷新、购买提交与场景重置。候选缓存仅在实现文件中可见 |
| `src/ResearchConfig.cpp` | 从显式传入的配置文件及引擎合并规则读取项目，保留文本原字节，检查未知字段、效果及前置链，重读时清理展示和效果缓存 |
| `src/ResearchEffects.cpp` | 科研效果校验、资金和转换执行、科技生产权限、生产价格与退款 Hook。生产权限和价格缓存由本模块清理 |
| `src/ResearchInternal.h` | 科研内部接口，以及查询被动注册、所属方资格和调用单位转换的运行时桥接；不暴露技能容器或转换实现 |
| `src/ResearchUI.cpp` | 科研展示、输入、图标解码与卡片缩放缓存；保留主动技能界面片段 |
| `src/HudFrame.cpp` / `src/starfall_hud_v1.h` | 为支持 GPU 的宿主输出科研与技能的透明 HUD。宿主在游戏线程上以 `Starfall_SetHudSinkV1` 注册回调；每次绘制由宿主选择原生、原生与 GPU 并行准备、或仅 GPU 输出。图片为逐行 straight RGBA8，按回调顺序做 source-over 混合；空帧撤下 HUD，失败帧整体丢弃。 |
| `src/ResourceCache.h` / `ResearchAudio.h` / `ResearchText.h` | 有界资源缓存及音频、文字展示工具 |
| `src/VisualScale.cpp` | 原生 SHP、VXL、弹体及附着动画的视觉缩放 |
| `src/SonicRipple.cpp` | 多目标声波的前沿绘制。原生输出折射战场像素，GPU 输出沿相同轨迹叠加半透明波纹 |
| `src/BuildingSpawn.h` / `BuildingSpawn.cpp` | 生成建筑的类型约束、完整占地检查、确定性邻近空地搜索和原生创建；无独立持久状态 |
| `src/BuildingTransform.h` / `BuildingTransform.cpp` | 完整占地一致性、建筑转换资格、原生对象创建与身份衔接；运行时回调重绑定缓存，已有按 ID 保存的被动和 Lua 状态继续使用 |
| `src/CarrierConversion.h` / `CarrierConversion.cpp` | 移动母体内置变形时调整子机槽位。游戏的子机管理器持有母子关联和补充计时，并负责存读档 |
| `modules/ActSp/` | 飞行子机、地面守卫、高级驻军、可建造高台，与主运行时共用游戏循环和存读档入口 |
| `src/Starfall.cpp` | 场景初始化与重置、每帧调用顺序、被动事件队列与分发、跨模块协调 |
| `src/PassiveTypes.h` / `PassiveRules.cpp` | 被动与动作定义、配置读取、持续状态和动作字段校验。验证脚本函数和拾取物 ID 时只读查询场景注册表 |
| `src/BuildingResearch.cpp` | 建筑与全局科研点收入、工程师和间谍触发的免费科研、免费选项与领取。拥有按建筑类型索引的规则缓存 |
| `src/ResearchDescription.cpp` | 科研卡片的条件、动作和组合效果说明。通过只读接口获取被动定义 |
| `src/ForceMove.cpp` | 强制位移、目标坐标与空中索敌、碰撞、落地和绘制。拥有移动记录与姿态恢复记录 |
| `src/runtime/PassiveRegistration.inc` | 注册被动、发现脚本、解析绑定、装载场景配置 |
| `src/runtime/PassiveStates.inc` / `PassiveGrants.inc` | 持续状态、属性修饰，以及光环、乘员、子机与临时被动的授予和回收 |
| `src/runtime/PassiveActions.inc` / `CombatHooks.inc` | 结果执行、变形与传送，以及引擎战斗事件采集 |
| `src/runtime/Savegame.inc` | 扩展存档流读写、历史版本校验、状态提交及存读档入口 |

## 接口与生命周期

外部调用科研使用 `Research.h`；内部模块和运行时桥接使用 `ResearchInternal.h`。科研模型不包含 YRpp 头文件，可在独立进程中用测试边界替代引擎调用进行验证。跨模块的运行状态在 `Research.cpp` 定义一次，头文件只声明；现有存档代码继续访问相同类型和字段。

- 载入配置仍由被动注册完成后调用 `Research::readConfiguration(configPath)`，随后配置校验才能查询被动是否存在。
- 抽卡只通过 `Detail::matchSeed()` 读取共享对局种子，使用原来的局部随机算法；打开本地界面不推进游戏随机数。
- `SharedRaritySequence` 用对局种子和各玩家的 `completedRounds` 计算相同轮次的公共稀有度；具体项目仍独立抽取，缺少指定稀有度时按个人候选池补选。仅成功普通购买推进轮次，免费建筑科研不推进。轮次随存档保存，旧存档按已有科技总数迁移。
- 外交科研标题和科研公告统一通过 `player_name_policy.h` 生成玩家显示名。在 MRGA2 内调用宿主的 `MRGA_VisiblePlayerNameV1`；旧宿主没有该导出时用通用称呼，独立运行保持原先的玩家名。此过滤只作用于显示，不改写引擎名称或同步状态。
- [Tab 科研展示 C ABI v1](../src/starfall_research_display_v1.h) 供宿主在游戏线程读取有效玩家已完成的研究。列表按原生外交页的获得顺序排列，效果文字包含原生说明与触发条件，图标以 48×48 的直通道 RGBA8 复制到调用方。资源修订号在配置加载时更新，宿主在切图或读档后重新取列表和图标。查询只读已加载状态。
- 免费科研次数角标和下一份奖励按钮走同一 HUD 绘制路径。领取提示的光迹在原生输出中提亮现有画面，在 GPU 输出中使用透明叠加近似，沿用 HUD v1 接口。差异仅在显示，不影响领取逻辑或同步状态。
- 定点板条箱在独立运行时使用 Starfall 的最多 3 格避让规则；YRMod 替换了原生定点选址入口，保留自己的最多 24 格确定性搜索。两条路径都经过 `SF_CheckCrateCellForPickup` 的最终落格检查：已有拾取物的候选格失败，不占用箱子槽，平台继续按原顺序检查下一格。
- Ares 3.0p1 的自定义火箭分支从 `0x663218` 返回到 `0x6632CC`，跳过 Starfall 在 `0x66321D` 和 `0x66328C` 的原生命中 Hook。普通火箭经过这两个 Hook；自定义导弹仍经过通用弹体和绘制 Hook，但该 Ares 分支不应用 Starfall 的原生火箭火力（Firepower）修正。
- `Research::clearOfferCache()` 同时清除候选、生产价格和科技生产权限缓存，保持购买、读档及配置重读后的失效关系。
- `Research::reset()` 清理科研运行状态和展示资源，停止旧场景声音；场景重置同时调用 `BuildingResearch::clearTypeCaches()` 清理建筑类型缓存。
- 模块拆分不调整 `SF_Frame` 中各系统的调用顺序，也不改变存档标记、字段顺序或异常存档的事务提交方式。
- 特殊行为每帧在主运行时之后执行。沿用 `ActSp.GuardPoints` 和 `ActSp.HighPlatforms` 存档流以读取旧版状态。

## 构建与检查

`tools/module-sources.cmd` 维护共用编译单元清单，供正式构建、游戏探针和完整运行时单元测试调用。增加新的 `.cpp` 时更新该清单，不把实现重新包含到主文件。`hex-icons` 探针直接包含界面实现，因此其主运行时入口由测试构建脚本单独选择。

- `build.cmd`：构建正式 DLL。
- `tests/unit/build.cmd`：运行独立测试程序。`HudFrameTest` 验证透明叠加、局部卡片、裁剪、帧顺序和失败处理。`ResearchModelTest` 验证确定性、配置顺序变化、所属方隔离、刷新、购买失败及重置。
- `tests/run-game.ps1`：验证引擎效果、界面、新游戏和实际存读档。
- `tests/run-multiplayer.ps1`：验证两个真实客户端的同步状态。

## 共享场景状态

`src/runtime/` 下的实现片段随主运行时编译，共用一份场景状态。新增动作在 `PassiveRules.cpp` 校验字段，在 `PassiveActions.inc` 实现执行。新增持久状态须同时检查 `Starfall.cpp` 中的场景重置和 `Savegame.inc` 中的历史版本读取。每帧子系统的执行顺序集中在 `SF_Frame`。
