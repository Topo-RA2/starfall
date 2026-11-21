# Starfall Lab / 流火实验室

<img src="assets/brand/starfall-lab-comet-fragments.png" alt="流火实验室正式图标「星裂」" width="96" height="96">

Starfall Lab（流火实验室，简称流火）是《红色警戒 2：尤里的复仇》的 32 位 DLL 扩展。通过 INI 配置单位被动、科研选项、国家限制和前置项目。玩家可通过地图触发器或建筑获取科研点，每 100 点购买一项研究。

当前版本：**0.38**。支持事件触发与持续状态、可叠加被动、信号联动，以及可配置的科研三选一。自定义点选超武可用 `Type=ActionSequence` 执行 INI 结果和 Lua 脚本。[超武配置示例](examples/ActionSequence/Rules.ini) · [规则手册](https://windswhisper.github.io/starfall/docs/manual.html)

![科研三选一界面示意图](docs/images/research-selection.png)

*科研界面示意，具体项目与效果由 INI 配置决定。*

项目名称已更新，现有的 `Starfall.dll`、`Starfall.ini` 和 INI 中的 `Starfall.*` 字段继续使用原名称，以兼容已有配置和存档。

## 在 Windows 上编译

安装 Git 和 Visual Studio 2022（Community 或 Build Tools）。勾选“使用 C++ 的桌面开发”，包含 MSVC v143 x86/x64 工具和 Windows SDK。编译本身无需安装游戏。

```bat
git clone --recurse-submodules https://github.com/windswhisper/starfall.git
cd starfall
.\build.cmd
```

产物为 `build\Starfall.dll`。如果已普通克隆仓库，可直接运行脚本补齐 YRpp。更新源码后用 `git submodule update --init` 同步依赖到项目记录的提交。GitHub 的 Download ZIP 不含子模块，请使用 Git 克隆。

源码职责、状态所有权和模块构建清单见 [模块边界说明](docs/architecture.md)。

## 运行与测试

流火加载时要求 `gamemd.exe` 的 CRC 为 `0x54CC0A13`；不匹配时会拒绝加载。游戏本体及加载器需自行准备。

### Ares 与 Phobos 兼容性

- **Ares**：可与公版 Ares 配合使用，流火的单位转换不依赖 Ares。永久心控免疫针对 Ares 3.0p1 适配；其他版本或构建的兼容性需要验证，不匹配时会在 `Starfall.log` 记录。
- **Phobos**：可与公版 Phobos 配合使用。未加载 Phobos 时，单位转换使用流火的内置实现；加载 Phobos 时，需要该版本提供 `ConvertToType_Phobos` 公共接口。若接口不存在，单位转换不可用，并在 `Starfall.log` 记录原因。不同版本和其他扩展组合的兼容性需要分别验证。

1. 把编译出的 `Starfall.dll` 放在游戏根目录，与 `gamemd.exe` 同级。可在游戏实际加载的 `rulesmd.ini` 中配置流火，也可自行创建 `Starfall.ini`。默认科研面板、图标和示例音效已打包在 DLL 中，无需另行复制。自定义 PNG 图标和 WAV 音效需按配置放置。
2. 使用与所选扩展版本匹配的 Syringe 或 SyringeEx 启动游戏。与 Phobos 同时使用时，应让 Starfall 先于 Phobos 加载，以保持心控免疫生效。战场右上角的图标打开科研面板。
3. 可选：将 [tests/maps](tests/maps) 中的地图放入客户端的 `Maps/Custom/`。在 `Starfall - Research Points Test` 选择 1 号出生位，地图触发器先给 90 点，在 `[CAHOSP]` 配置 `Starfall.ResearchPoints=10` 和 `Starfall.ResearchInterval=150` 后，科技医院运行 150 帧即可补齐到 100 点。

查字段、默认值和配置示例，可以访问 [在线规则手册](https://windswhisper.github.io/starfall/docs/manual.html)，也可以下载项目后用浏览器打开 [中文 HTML 查询手册](docs/manual.html)。面向 AI 和模组开发者的 [英文 Markdown 参考手册](docs/manual.md) 按当前实现整理了字段类型、默认值、适用范围、Lua 接口和配置示例。保持科研项目 ID 稳定可以让后续版本继续识别存档中的研究记录。

其他 32 位游戏扩展可调用 `Starfall_GetOwnedResearch`，按玩家读取已获得的科研项目 ID。[公共头文件](src/StarfallAPI.h) · [调用示例](docs/manual.md#native-research-query)

例如，下面的 `Starfall.ini` 让灰熊坦克每击杀一个敌人恢复 50 点生命：

```ini
[Starfall.PassiveTypes]
0=SF_KillRepair

[SF_KillRepair]
Trigger=OnKill
Filter.TargetHouse=Enemies
Action=Heal
Action.Amount=50

[MTNK]
Starfall.Passives=SF_KillRepair
```

把示例合并到现有配置时，请使用空闲的注册编号，并把被动 ID 追加到单位已有的 `Starfall.Passives` 列表。更多科研配置见 [海克斯玩法示例](examples/Starfall.HexBrawl.ini)。自定义单位、武器与弹头统一放在 [HexBrawl.rules.ini](examples/HexBrawl.rules.ini)，合并到 rulesmd.ini 或地图规则；旧的 Bunker、SpiderBomb 两份规则已合并，不再单独维护。

“超级防御塔”需要本次更新的 Starfall.dll；另需将 [HexBrawl.art.ini](examples/HexBrawl.art.ini) 合并至原有 artmd.ini，以提供独立占地，不能用此片段覆盖完整 art 文件。研究后九种常规武装防御建筑改为生产巨型型号，包含法国巨炮，保留原有建造前置。已摆放的普通塔不变，未建成订单取消退款。占地长宽各翻倍（面积四倍），巨炮变为 4×4。

新增五项海克斯：买一送一（普通，成功放置磁暴线圈或防空炮后在附近 5 格内赠送普通哨戒炮）；动力涡轮（普通，盟军发电站产电 ×1.5）；变废为宝（稀有，磁电坦克获得敌方击杀归属后掉落原生随机奖励箱）；电力盈能（普通，每完整 500 点余电给耗电防御塔 +10% 射速，上限 +50%）；光棱大兵（史诗，GI 价格 ×2、装甲 ×1.5，部署武器伤害 32、射程 7，精英射程 8）。前两项防御塔效果兼容对应巨型塔；赠塔不会因旧建筑、占领、变形、技能生成或读档而重复触发。需要更新 DLL、选用一版 Starfall.ini，并合并配套 rules 片段。光棱大兵的专用武器通过 Ares 的 WeaponTypes 注册，与现有缩小射线采用相同依赖。

新增六项海克斯：复制中心（史诗，盟军与苏联解锁复制中心）；我也有巨炮（稀有，非法国国家解锁巨炮，可与超级防御塔组合）；咔！BOOM！（普通，伊文炸弹伤害与半径 ×2）；武装吧矿车（史诗，每辆武装采矿车终身固定一种随机高级武器，精英时沿用同类精英武器）；大火箭（史诗，V3 火箭生命、伤害、体型与爆炸半径 ×2）；放烟花（稀有，炸弹引爆后每 3 帧在 2 格内追加一次爆炸，共 8 次，每次为主爆炸基础伤害的 20%）。放烟花需要把 [HexBrawl.lua](examples/HexBrawl.lua) 复制到游戏根目录；伊文阵亡不打断，拆弹不触发，定时任务可存读档。请同步更新 DLL、INI 与配套 rules。

新增五项海克斯：超级防弹衣（史诗，所有己方步兵装甲 ×3、移速 ×0.85）；华佗秘卷（稀有，己方步兵每 30 帧恢复 25 生命）；混沌命运（史诗，随机获得 2 项全稀有度科技）；质变史诗级（稀有，随机获得 1 项史诗科技）；质变稀有级（普通，随机获得 1 项稀有科技）。随机奖励排除自己和已拥有项，允许抽中其他奖励科技继续发奖，不额外扣科研点或推进普通轮次；存读档不会重复发奖。请同步更新 DLL 和配置。

新增六项科技：好兄弟（3 格内每个其他友方动员兵提供 5% 攻击和装甲，上限 50%）、捡到宝（立即获得 15000 金钱）、单兵核武器（防空步兵攻击时 20% 概率追加小型核弹，基础伤害固定 120，使用 TerrorBombWH）、遗志（阵亡动员兵 3 格内友方步兵恢复 40 生命并获 40% 攻速持续 75 帧）、狙击手和去月球。六项科技均配有专属图标。

分叉电流、食腐、大脑扰乱、心灵强化和欸？大狗！各有独立科技图标。食腐需要将更新后的 HexBrawl.lua 复制到游戏根目录。

新增五项战术海克斯：连锁反应（稀有，恐怖机器人击杀敌方载具时，在死者位置造成半径 1.5 格、90 点基础伤害的电磁爆炸，仅敌方载具瘫痪 15 帧，复用精英基洛夫 KTSTLEXP 特效）；战地回收（稀有，己方所有地面载具死亡返还基础价格 30%，含矿车、运输车和基地车，出售、变形、消耗不触发）；磁悬浮加速（普通，遥控坦克移速和射速 ×1.3）；红色信念（史诗，鲍里斯装甲 ×1.5，为自身及 4 格内己方、盟友提供伤害 ×1.3 与新心控免疫，光环不叠加）；敏捷身手（史诗，谭雅每次敌方攻击独立 70% 闪避，闪避无冷却；成功闪避后优先选择 12 格内攻击她的可识别敌方地面载具，否则选择最近一辆，传送至附近空地。瞬移不附带超时空冻结；仅成功瞬移有 45 帧冷却，无目标或空地时只闪避）。需同步更新 DLL、所选 INI、HexBrawl.rules.ini 与 HexBrawl.lua。

海克斯另提供 [PCX 版配置](examples/Starfall.HexBrawl.PCX.ini)，配套 [HexBrawlIconsPCX](examples/HexBrawlIconsPCX) 中的 85 张图标。将其中的 hx_*.pcx 文件直接复制到游戏根目录，不要另放在子目录中。选择 PNG 或 PCX 其中一版作为 `Starfall.ini`，或合并到已有配置。两版玩法相同，仅图标路径不同；PCX 为 96×96、256 色，洋红色透明，不保留半透明。可用 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/art/hex-pcx.ps1` 从 PNG 版重新生成。

「超级要塞」（史诗、盟军）把现有及之后生产的 `BFRT` 自动变形成 `SF_HX_SuperFortressUnit`，保留载员和等级。升级后容量为 8，步兵和坦克各占 1 格（`SizeLimit=6`），禁止要塞互装；装甲与体型 ×2，移速 ×0.70，价格增加 200%（远征基价 2500 → 7500，其他价格效果另行叠加）。仍从原战斗要塞图标生产，新单位复用 BFRT 美术。必须同时合并配套规则中的 `[VehicleTypes]` 注册和新单位段，并更新 DLL（修正按人数计容量的满载变形检查）；转换使用现有 Phobos 公共接口。其他 MOD 应核对新单位段内的远征 BFRT 基础数值。

「要塞直升机」（稀有、盟军）把现有及之后生产的 `SHAD` 自动变形成开放舱型号 `SF_HX_FortressHeliUnit`，保留载员、等级和原有基础属性。载员按自身武器规则在机舱内朝外射击；沿用夜莺的 5 格运载量、1000 基价及原武器。仍从夜莺图标生产，复用 SHAD 美术。需合并配套规则中的新单位注册和单位段。

## Lua 技能

将 [示例 INI](examples/Lua/Starfall.ini) 合并到现有配置，把这四个示例的 .lua 文件复制到游戏根目录。INI 定义兵种、触发、冷却和技能数值，Lua 函数通过 `ctx.params` 读取自定义参数。例如部署美国大兵时，[FieldCommander.lua](examples/Lua/FieldCommander.lua) 会晋升范围内可训练的友军。[事件技能参数](https://windswhisper.github.io/starfall/docs/manual.html#lua-skills)

[ActiveBarrage.lua](examples/Lua/ActiveBarrage.lua) 和 [DirectionalShot.lua](examples/Lua/DirectionalShot.lua) 为灰熊和 V3 添加左下角主动技能按钮。前者选圆心，由最近的一辆车轰炸；后者选方向，所有就绪车辆分别发射炮弹。[主动技能参数](https://windswhisper.github.io/starfall/docs/manual.html#lua-active)

[龙卷风示例](examples/Tornado/Starfall.ini) 给灰熊增加选方向发射的主动技能。将 [Tornado.lua](examples/Tornado/Tornado.lua) 放入游戏根目录，把示例的注册项和技能段合并到规则，并将 `SF_Tornado` 追加到灰熊的被动列表。选中灰熊并点击一个方向。风眼默认在车前 5 格形成，沿直线推进到距发射点 18 格处，遇地图边缘缩短行程。每 2 帧卷入沿途半径 4 格内的步兵和载具，不分阵营，最多同时携带 12 个。最早卷入的单位在 96 帧内绕移动风眼旋转 5 圈，后来卷入的单位随风走完剩余路程。VXL 载具朝位移方向倾斜，风停后用 12 帧落地。每 6 帧对已卷入单位和此时风眼内的建筑造成 12 点基础伤害，共 16 次；建筑留在原位。

[AreaBarrage.lua](examples/Lua/AreaBarrage.lua) 演示定时回调和同步随机。V3 开火后锁定目标区域，每 4 帧引爆一次，共 20 次。任务和已抽取的随机进度可存读档。[定时任务接口](https://windswhisper.github.io/starfall/docs/manual.html#lua-timers)

## 源码与依赖

- `src/` 是正式 DLL 源码，`assets/research/` 提供科研界面美术。
- `assets/sound/`、`assets/research/` 中的内置资源由构建脚本打包进 DLL；`examples/` 包含示例配置和外部图标。
- 独立的实机测试、配置和地图在 [tests/](tests/README.md)。
- Lua 5.4.8 运行库取自 [Lua 官方源码标签](https://github.com/lua/lua/tree/v5.4.8)，源码与许可保存在 `references/lua/`，构建时静态链接进 DLL。
- [YRpp](https://github.com/Phobos-developers/YRpp) 通过 Git 子模块固定版本，提供编译所需的游戏接口头文件。项目开发参考了 [Phobos](https://github.com/Phobos-developers/Phobos)。
- Starfall Lab 自有代码采用 GNU 宽通用公共许可证第 3 版（`LGPL-3.0-only`）。完整条款见 [LGPLv3 正文](LICENSE) 和其引用的 [GPLv3 正文](COPYING)。外部依赖保留其上游声明。游戏本体、第三方 DLL 和加载器需自行准备。
