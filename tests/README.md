# Starfall 测试

测试分为快速检查、单客户端实机测试和真实双客户端联机测试。统一入口是 `tests/run.ps1`，用例清单与预期结果集中在 `tests/cases.json`。当前包含 43 个引擎用例和 6 个独立测试程序；测试没有因编译缓存而跳过执行。

## 常用命令

在项目根目录运行：

```powershell
# 不需要游戏：测试工具自检 + 6 个独立程序
.\tests\run.ps1

# 查看分组、用例或执行计划，不编译、不启动游戏
.\tests\run.ps1 -List
.\tests\run.ps1 -Suite smoke -Plan

# 日常改动：快速检查及关键实机链路
.\tests\run.ps1 -Suite smoke

# 只测这次改动涉及的功能
.\tests\run.ps1 -Case cloakable,condition-formula
.\tests\run.ps1 -Suite lua
.\tests\run.ps1 -Suite research

# 两个真实客户端，逐帧比较
.\tests\run.ps1 -Suite network
.\tests\run.ps1 -Suite network -NetworkProfile Impaired -Seed 8642097

# 全部功能回归
.\tests\run.ps1 -Suite full
```

默认遇到第一个失败即停止，返回失败状态。需要收集后续独立用例的结果时，使用 `-ContinueOnFailure`；整个分组仍会报告失败。所有实机用例按顺序运行，避免界面和联机端口互相干扰。

兼容 PowerShell 7 和 Windows PowerShell 5.1。若后者按本机执行策略拒绝直接加载脚本，可使用 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/run.ps1`，仅为本次进程允许运行本地测试脚本，不修改系统策略。

## 首次配置

编译需要 Visual Studio 2022 的 x86 C++ 工具和 Windows SDK。游戏测试需要兼容的 `gamemd.exe`、游戏素材及 SyringeEx；加载器必须支持显式 DLL 列表、`--args` 和 `--nodetach`。

复制 [environment.example.json](environment.example.json) 到 `tests/local/environment.json`，填写本机路径。该目录受 Git 忽略，不会提交游戏资源或本机设置。也可直接传入参数：

```powershell
.\tests\run.ps1 -Case cloakable `
    -GameDirectory 'D:\Games\Yuri' `
    -PhobosPackageDirectory 'D:\Packages\Phobos'

.\tests\run.ps1 -Suite network `
    -GameDirectory 'D:\Games\Yuri' `
    -PhobosPackageDirectory 'D:\Packages\Phobos' `
    -StandaloneSpawner 'D:\Packages\CnCNet-Spawner.dll'
```

`-GameDirectory` 未提供时，优先读取本机配置，其次尝试 `tests/local/game-base`。游戏资源由用户自行提供。当前工作区已将一份可复用的运行资源整理至此目录，历史运行目录不再承担素材来源的职责。

单客户端 `action-sequence`、`immune`、`cloakable`、`spawn-building`、`building-transform`、`force-move` 可加 `-Standalone`，只加载 Starfall 和独立 Spawner。其余用例保留各自的扩展依赖。联机仍需要完整的 Ares、Phobos 和独立 Spawner。已有直接入口继续可用：

```powershell
.\tests\run-game.ps1 -Case cloakable -GameDirectory 'D:\Games\Yuri'
.\tests\build.cmd cloakable
.\tests\unit\build.cmd
```

## 分组与覆盖

| 分组 | 主要内容 |
| --- | --- |
| `quick` | 测试目录和清单一致性、断言与失败识别、编译缓存失效、素材隔离和清理边界；Lua 运行库、音频、文字缓存、资源缓存、科研模型、动作参数 |
| `smoke` | quick，加公式与存档、持续隐形、Lua 技能、科研价格、ActionSequence |
| `passives` | 条件、获得事件、层数、续期、光环、乘客/子机支援、信号、晋升、部署、免疫、隐形、伤害/开火拦截、多目标、射速、射程和生命 |
| `lua` | 脚本动作、定时任务、状态、动画、主动技能、轰炸、地图脚本和超武脚本 |
| `research` | 界面、费用、生产权限、免费奖励、建筑升级、转换和科研授予 |
| `visuals` | 图标、被动动画/染色、弹体与龙卷风 |
| `movement` | 强制位移、超时空移动与龙卷风 |
| `network` | 两名真实玩家、技能命令、科研、随机数、定时任务、动画、位移及临时隐形；881 帧原生与扩展状态对比 |
| `full` | quick + 所有功能用例，含联机；耗时明显更长 |

42 个单客户端用例均在清单中说明地图、存档、日志、阶段和必须出现的成功记录。通常执行新局及读档两阶段；图标用例仅新局，转换保留原有扩展组合及额外阶段。详细用例名通过 `-List` 查看，测试源码在 `game/<用例>/Probe.cpp`。

`force-move` 还覆盖移入／移出敌方射程、实际弹体瞄准、跨区域威胁迁移、探测范围和相邻格登记，以及读档后步兵／载具的坐标同步。击飞测试覆盖可配置的空中门槛、对空／对地索敌切换、抬高瞄准点、导弹实际命中、低空伤害、避免重复溅射伤害、空中登记迁移与清理、读档恢复。联机技能包含击飞，采样比较原生威胁、空中登记、目标高度及空中判定。

`building-transform` 验证占地格集合、原生身份与归属计数、生命比例、临时被动和层数、Lua 同步回调及定时任务、原持续时间和存读档；联机用例也执行两次建筑变形并检查脚本句柄连续性。

`hex-icons` 还验证主动技能 PNG 指示器的地面投影、透明度、亮蓝/红色状态、视口裁剪、缺图回退及缓存释放，并对比不同长度下箭头首尾的实际像素，确认只有中段伸缩。动画检查覆盖自转后的地面范围与中心、弹性缩放幅度及中心、循环衔接，并输出固定时间采样的预览帧。`active-skills` 验证贴图配置解析、原有施法及存读档，并保留范围和方向预览截图。

PNG 透明度检查还覆盖主动按钮的可用/冷却/缺图回退状态、科研卡片的全透明/半透明/不透明像素、缓存复用和边界裁剪，并在非纯色背景上验证卡片缩放后透明区域显示目标位置的地图，不复制原位置的地图像素。

`spawn-building` 属于 `passives`、`lua` 和 `full`：覆盖完整建筑占地、局部阻挡、最近空地、地图边缘、水面/坡地/桥梁、半径与类型参数校验、INI/Lua 调用、所属方、电力和存读档。`multiplayer` 同时验证两位玩家从 Lua 生成建筑时的落点和归属同步。

## 编译与运行效率

- `tests/build.ps1` 按源码内容、工具链签名及输出文件摘要检查缓存。文件同长度、时间戳未变、产物被替换等情况也能识别；不依赖“存在 DLL 就跳过”。
- Lua 库、科研资源、公共模块和界面对象存放在 `build/tests/shared`，供用例复用；变化的探针单独编译。测试仍执行真实断言，缓存的只是编译产物。
- `-Rebuild` 强制重建选中的测试程序；公共依赖在内容变化时自动重建。构建锁避免同时写入共享缓存。
- 同盘支持时，独立运行目录通过硬链接共享只读 `.mix` 素材；不支持时自动复制。DLL、INI、Lua、地图和存档各自隔离。需要完全复制时使用 `-CopyGameAssets`。
- 用例无论通过还是失败，通常都会释放运行目录的游戏素材链接/副本，保留诊断文件。需要完整现场时使用 `-KeepGameAssets`。
- `run-multiplayer.ps1 -SkipBuild` 现在会验证缓存确实匹配；源码改变或产物缺失时会拒绝运行旧测试 DLL。

不要并发运行多个实机分组或使用相同端口的联机测试。需要单独验证工具本身时运行 `tests/TestInfrastructure.ps1`，无需游戏或编译器。

## 结果与排错

每次统一运行产生独立的 `tests/local/records/suite-<时间>-<ID>/`：

- `summary.json`：请求的用例、实际执行数、通过/失败、耗时和日志路径。
- `<用例>.log`：完整构建及运行输出。
- `<用例>.json`：各阶段结果或独立程序的退出状态。

实机现场保留在 `tests/local/runs/<用例>-<ID>/`，含配置、测试 DLL、每阶段日志、`result.json` 和素材共享记录。新的报告不会覆盖之前分组的证据；旧的 `local/records/<用例>-fresh.log` 等文件继续作为最新结果的兼容入口。

程序退出异常、探针出现 `FAIL`、缺少必要成功记录、超时或两端状态不同都会判失败。联机 `-InjectDivergence` 是故意制造差异的反例，预期失败；不要将其计为正常功能失败。弱网模式保留延迟、丢包与短时断网检查。

## 清理

```powershell
# 先预览；不删除任何文件
.\tests\clean.ps1

# 删除历史运行中可重新准备的游戏素材副本
.\tests\clean.ps1 -Apply

# 同时清理旧的独立构建中间文件；共享缓存和可执行产物保留
.\tests\clean.ps1 -Apply -Intermediates
```

清理只处理工作区中具有测试标记的运行目录和明确的编译中间文件，先保存完整清单，再逐项删除；遇到运行中的测试、越界路径或目录链接会拒绝。不会删除 `game/` 用例源码、独立程序源码、地图、存档样本、测试日志、配置、验证报告、`local/game-base` 或正式 `build/Starfall.dll`。

`local/legacy/historical-tests.zip` 保留早期测试资料，`local/saves` 保留历史存档样本，`maps/` 保留手动测试地图。测试 DLL 仅在带 `Starfall.test` 标记的隔离目录执行自动化动作，不应作为正常游戏版本安装。

## 正式产物检查

正式构建仍使用项目根目录 `build.cmd`。Hook 冲突检查继续通过 `check-hook-ranges.ps1` 执行。资源检查使用 Windows PowerShell 5.1：

```powershell
powershell.exe -NoProfile -Command 'Add-Type -Path tests/resources/ResourceCheck.cs -ReferencedAssemblies System.Drawing; [ResourceCheck]::Check((Resolve-Path build/Starfall.dll).Path)'
```

测试覆盖依赖实际运行的用例和扩展版本。快速分组不能替代与改动相关的专项测试；通过单机测试也不能替代联机验证。
