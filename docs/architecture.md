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
| `src/BuildingSpawn.h` / `BuildingSpawn.cpp` | 生成建筑的类型约束、完整占地检查、确定性邻近空地搜索和原生创建；无独立持久状态 |
| `src/BuildingTransform.h` / `BuildingTransform.cpp` | 完整占地一致性、建筑转换资格、原生对象创建与身份衔接；运行时回调重绑定缓存，已有按 ID 保存的被动和 Lua 状态继续使用 |
| `src/Starfall.cpp` | 被动配置和运行时、事件分发、建筑科研适配、被动说明生成、引擎 Hook、存读档及模块协调 |

## 接口与生命周期

外部调用科研使用 `Research.h`；内部模块和运行时桥接使用 `ResearchInternal.h`。科研模型不包含 YRpp 头文件，可在独立进程中用测试边界替代引擎调用进行验证。跨模块的运行状态在 `Research.cpp` 定义一次，头文件只声明；现有存档代码继续访问相同类型和字段。

- 载入配置仍由被动注册完成后调用 `Research::readConfiguration(configPath)`，随后配置校验才能查询被动是否存在。
- 抽卡只通过 `Detail::matchSeed()` 读取共享对局种子，使用原来的局部随机算法；打开本地界面不推进游戏随机数。
- `Research::clearOfferCache()` 同时清除候选、生产价格和科技生产权限缓存，保持购买、读档及配置重读后的失效关系。
- `Research::reset()` 清理科研运行状态和展示资源，停止旧场景声音；建筑类型缓存仍由主运行时在场景重置时清理。
- 模块拆分不调整 `SF_Frame` 中各系统的调用顺序，也不改变存档标记、字段顺序或异常存档的事务提交方式。

## 构建与检查

`tools/module-sources.cmd` 维护共用编译单元清单，供正式构建、游戏探针和完整运行时单元测试调用。增加新的 `.cpp` 时更新该清单，不把实现重新包含到主文件。`hex-icons` 探针直接包含界面实现，因此其主运行时入口由测试构建脚本单独选择。

- `build.cmd`：构建正式 DLL。
- `tests/unit/build.cmd`：运行独立测试程序。`HudFrameTest` 验证透明叠加、局部卡片、裁剪、帧顺序和失败处理。`ResearchModelTest` 验证确定性、配置顺序变化、所属方隔离、刷新、购买失败及重置。
- `tests/run-game.ps1`：验证引擎效果、界面、新游戏和实际存读档。
- `tests/run-multiplayer.ps1`：验证两个真实客户端的同步状态。

## 仍待逐步拆分

被动技能解析、动作执行和状态更新、存档事务、建筑科研与被动说明仍在主文件；`.inc` 片段也尚未全部独立编译。后续先为相关状态建立接口，再分别迁移；尤其存档应继续先完整校验再提交，帧内系统顺序应保持稳定。本轮不将这些剩余耦合视为已经解决。
