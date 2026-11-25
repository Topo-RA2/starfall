# 流火示例

三个入门示例都使用《尤里的复仇》原版单位和流火内置界面。任选一项，把该目录的 Starfall.ini 放在已加载 Starfall.dll 的游戏根目录，然后开始新对局。已有同名配置时，请合并对应段，给注册表分配空闲编号，并保留单位已有的被动列表。

| 示例 | 安装文件 | 在游戏里观察 |
| --- | --- | --- |
| 击杀后维修 | [Passive/Starfall.ini](Passive/Starfall.ini) | 用一辆受损的盟军灰熊坦克击杀敌人，坦克恢复 50 点生命，最多到满血。 |
| 一次科研三选一 | [Research/Starfall.ini](Research/Starfall.ini) | 开局获得 100 科研点。打开科研面板，可选 500 资金、灰熊降价 20% 或步兵降价 10%。选中一项后核对资金或生产栏报价。 |
| 武器脚本双发 | [WeaponScript/Starfall.ini](WeaponScript/Starfall.ini) 和 [DemoDoubleShot.lua](WeaponScript/DemoDoubleShot.lua) | 两个文件都放在游戏根目录。命令普通灰熊攻击移动中的目标，第一炮瞄准单位，6 帧后第二炮打向原格子。按 Ctrl 对地攻击时，两炮都打向该格。 |

## ActSp 示例

[灰熊警戒哨站](GrizzlyOutpost/README.md)使用机枪碉堡外观，设置集合点后派出一辆低空灰熊。目录中包含规则片段、美术配置和完整的灰熊子机模型。

[灰熊地面哨站](GrizzlyGroundOutpost/README.md)派出一辆原版灰熊坦克，沿地面绕障前往集合点。坦克阵亡后，哨站会在补员时间到达后重新派车。

[混凝土高台](HighPlatform/README.md)是可以建造、出售和摧毁的苏军高台。它将 4×4 地形升高四级，上方可以放置建筑和单位。

[中文规则手册](../docs/manual.html) · [英文参数参考](../docs/manual.md)
