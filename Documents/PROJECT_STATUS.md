# Glimmer 项目状态与长期工作台

> 这是本项目跨设备、跨会话的执行入口，不是完整技术文档。
> 开始工作前先阅读本文；需要实现细节时再查阅 `README.md` 中对应的功能章节与 `ARCHITECTURE.md`。

## 文档状态

- 最近更新：2026-09-30
- 当前分支：`main`
- 当前构建环境：Visual Studio 2026、v145、Windows x64
- 当前默认验证配置：`Debug | x64`
- 当前主线：P16 大尺度地形真实地貌重构
- 主线状态：进行中（条件分形 v2 与视觉对照已完成；阶段方案已细化，当前实施项为 P16-A 近远分层高度与尺度契约）

## 使用与更新规则

### 每次开始工作

1. 阅读“当前主线”，确认目标、范围和验收条件；
2. 检查 Git 状态，保护用户已有修改；
3. 阅读 `ARCHITECTURE.md` 和 README 中与当前任务对应的最新章节；
4. 只推进一个主线目标，除非用户明确调整优先级；
5. 如果当前主线被阻塞，从“后续任务”中选择不依赖该阻塞项的工作，并记录原因。

### 每次完成任务

1. 按任务的验收条件完成构建、运行或数据往返验证；
2. 将任务从“当前主线”或“后续任务”移动到“已完成里程碑”，不要复制保留；
3. 记录完成日期、核心结果、验证证据和对应提交；尚未提交时写“提交：待提交”；
4. 将优先级最高且依赖已满足的任务提升为新的“当前主线”；
5. 更新“已知问题与技术债”；已经解决的问题应删除或转入完成记录；
6. 同步审查 `PROJECT_STATUS.md`、`ARCHITECTURE.md` 和 `README.md`，按各自职责更新；无需修改的文档也必须确认仍与源码一致；
7. 架构、依赖、职责或数据流变化时更新 `ARCHITECTURE.md`，只写已经落地的事实；
8. 已实现功能行为、工作流或实现说明变化时更新 README；README 不包含未实施计划、设计建议、候选验收指标或任务顺序，统一放在本文件；新增功能章节必须位于 `## KB` 上方并沿用既有标题格式；
9. 允许调整本文结构、合并旧记录或拆分任务，只要当前目标和依赖关系更清晰。

### 三份文档的职责

| 文档 | 唯一职责 | 每次任务结束时检查 |
| --- | --- | --- |
| `Documents/PROJECT_STATUS.md` | 当前主线、实施计划与设计方案、后续顺序、完成记录、验收证据和技术债 | 任务状态、方案边界和下一步是否准确 |
| `ARCHITECTURE.md` | 当前已经实现的模块职责、依赖、生命周期、数据流和边界 | 源码结构或跨层关系是否变化 |
| `README.md` | 已实现功能演进、使用方式、实现笔记、验证结果和 KB；不保存计划 | 对外行为或开发工作流是否变化 |

三份文档允许互相引用，但不要复制整段内容。发生冲突时，以源码事实为准并在同一任务内修正文档。

### 状态定义

| 状态 | 含义 |
| --- | --- |
| 待开始 | 目标和验收条件已明确，可以直接实施 |
| 进行中 | 已产生代码或资源修改，尚未完成全部验收 |
| 阻塞 | 存在明确外部依赖，并已记录解除条件 |
| 已完成 | 实现与验收均完成，已经移入完成区 |

## 当前主线

### P16：大尺度地形真实地貌重构

**状态**：进行中。第一阶段已完成并验证；用户已明确将大尺度地形提升为当前优先级。

**目标**

- 在常用编辑器视野内呈现更大、更可信的山地尺度，参考 AfterglowRender 的地形构图与主流引擎的分块/LOD 思路；
- 保持已有场景的噪声语义和水平尺寸兼容，地形、水面、模拟和阴影共享同一世界范围。

**阶段顺序与验收**

2026-09-23 已完成的 v2、预设高度重标定及固定视角验证仅保留在“已完成里程碑”。下列为待实施工作；详细数据契约、子步骤、候选预算和验证矩阵见本节下方“P16 分阶段实施与优化方案”。规划完成不代表实现完成。

| 阶段 | 状态/依赖 | 交付与进入下一阶段的条件 |
| --- | --- | --- |
| P16-A 近远分层高度与尺度契约 | **当前实施项**；待实施 | A1 冻结基线与采样/单位契约；A2 统一坡角、曲率与版本兼容；A3 定义 Base/Detail/Simulation Delta、缓存预算与低分辨率模拟映射。解析地形、旧场景往返和资源预算验证通过后进入 B |
| P16-B 局部高度与连续几何 | 待开始；依赖 A | B1 无细节的 Tile 等价路径；B2 确定性局部细节、边界 Halo 与缓存回退；B3 屏幕误差 LOD/Morph 与 Color/Shadow/Water 共用表面；B4 可选模拟降采样。接边、重载、移动视角、质量预算及近景轮廓通过 |
| P16-C 山系与排水骨架 | 待开始；依赖 A/B | C1 新版本区域/山脊/岩性；C2 洼地、湖泊、出口与流向；C3 汇流累积及河谷剖面。非湖泊路径全部终止于合法出口/湖泊，无环路，跨块一致；灰模体现主支河谷和山麓过渡 |
| P16-D 创作侵蚀与持久生成配方 | 待开始；依赖 C | D1 稳定性/近干格速度核查；D2 有限次水蚀、沉积和热蚀；D3 侵蚀历史场及配方版本/保存/Undo。水与地形+泥沙守恒、Reset、重建复现通过；不隐式保存 Runtime |
| P16-E 静态地貌材质与预设 | 待开始；依赖 D | E1 正确曲率/汇流及侵蚀沉积权重；E2 近景片元派生采样与微细节；E3 五预设按地貌特征调参。四层权重有效且归一化，纹理不会掩盖灰模缺陷，LOD 不改变地貌归属 |
| P16-F 目标设备性能与总验收 | 待开始；依赖 B～E | 测生成、模拟、Color/Shadow/Water 帧时、缓存峰值及提交量；在目标显卡选默认质量，完成完整构建、回归、GPU 视觉/数值验收和三文档同步后才完成 P16 |

**范围与停止条件**

- 本轮以更可信山地、连接河谷、坡脚沉积和稳定近远景为交付；动态生态权重、低温积雪和植被实例化仍归 P14，不把 P14 总体验收并入 P16；
- 首版 Tile 仅承担渲染/派生数据；模拟保持全局网格，不实施跨 Tile 水文或相机驱动的局部独立侵蚀。预算/稳定性不达标时回退粗高度，保留世界表面一致性，暂停后续细节扩张；
- 本文件设计方案中的数值是候选验收门槛与资源估算，实施时需记录目标设备结果后定标；已有数据/GPU 契约不得放宽来换取视觉通过。

### P16 设计依据与源码审查

以下保存源码审查、外部方案对照及设计建议，没有新增实现或 GPU 视觉验收。现有条件分形已经形成区域与定向山带；主要差距集中在汇流结构、尺度一致性、近景数据/几何分辨率及地貌到材质的联系。

`GenerateFBM.comp` 的 valley 仍由噪声减高，`ErosionStrength` 在这里是形态参数。`DeriveTerrainMaps.comp` 的 Flow Potential 为中心到最低四邻居的落差，陡峭山脊也可能得到较大值，不能当作真实河流或湿度。Curvature 使用 `邻居和 - 4×中心`，碗形中心为正；材质却用负曲率提高湿度与 Soil，需统一符号契约后用解析碗形和凸丘验证。曲率也没有除以世界采样间距平方，分辨率改变会改变它的视觉意义。

热侵蚀 Talus 目前直接比较归一化高度差。建议后续以稳定坡角定义阈值：`normalizedTalus = tan(angle) × neighbourDistance / HeightScale`，斜向邻居使用 √2 倍间距，并处理零高度幅度。迭代次数和搬运速率也需要尺度验证，不能只修改阈值便宣称分辨率无关。

默认 WorldSize=1024、HeightMapResolution=1024 时，派生图使用约 1.001 单位间距；MeshResolution=256 经固定除数 3 得到 Chunk LOD0 的 86 格，4×4 Chunk 每块 256 单位，几何间距约 2.98，LOD1/2 约 5.95/11.64。Normal、Analysis 和 Weight 当前在顶点采样后插值，所以提高高度图分辨率不能独自解决轮廓与材质边界精度。Height 生成采用像素中心 UV，派生间距采用 `N-1`，分层/跨块设计应明确统一端点或像素中心契约。世界单位是否对应米也必须显式定义。

按当前源码纹理格式、不计 Mip、驱动开销和其它场景资源：生成器为双 R32F Height 加三张 RGBA16F 派生图，共 32 字节/格；水文四组 R32F Ping-Pong、两组 RGBA16F Ping-Pong 与三张 R32F 单图，共 76 字节/格；气候三组 R32F Ping-Pong 与四张 R32F 单图，共 40 字节/格。完整链总计 148 字节/格，1024² 约 148 MiB、2048² 约 592 MiB、4096² 约 2368 MiB，另有初始高度 CPU 缓存。该值是静态资源估算，不是显存实测；单张 Height 的成本不能代表整套 Terrain。

可借鉴的官方方案：

- [Unreal Landscape Technical Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine)：按 Component 保存高度，Sections 承担 LOD，重复边界顶点维持块间一致；[Landscape Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-overview) 说明 World Partition 的分块流送。Glimmer 已有绘制分块，但高度与模拟仍为整图，尚未拥有相同的数据分块能力。
- [Unity Terrain Tools Erosion](https://docs.unity3d.com/Packages/com.unity.terrain-tools@5.1/manual/erosion.html)：分别提供水蚀、热蚀和风蚀工具。可借鉴其创作阶段侵蚀工作流；不应把 Glimmer 非持久 Runtime 状态直接视作已保存地形。
- [Houdini HeightField Erode](https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_erode.html)：以 Erosion Feature Size 控制侵蚀尺度，可串接多个尺度，输出 height、sediment、debris、flow、flowdir 等层，并提供 Erodability Mask。它是地形创作工具，用于对照生成流程，不等同于实时引擎渲染架构。

建议保持 P16 当前顺序，先设计低频全局高度、按需高分辨率局部 Tile 与更低分辨率水文/气候网格的契约，明确缓存预算、世界坐标、边界 Halo、派生图和高度所有权。细节应以固定世界坐标确定性生成；局部 Tile 不独立重算整条低频地貌。增加细节不能切断既有河道，渲染高度、阴影和水面需要消费一致表面；将模拟 Delta 映射到细网格时要明确插值及体积守恒，不能简单复制格点水深。几何采用屏幕误差 LOD 与高度/边缘 Morph 后再验收过渡，Skirt 只遮缝。

之后建立显式出口/湖泊与洼地处理、流向及汇流累积，约束主河道与支流；加入受岩性/可侵蚀性控制的有限次 Authoring 水蚀与沉积，并提供显式 Bake 或可重建生成配方。再把侵蚀、沉积、真实湿度、坡向和温度用于静态/动态材质组合；微细节主要进入法线与粗糙度，陡壁/悬挑以局部 Mesh 补充高度场。保留现有噪声核，只有固定视角证据显示伪影或成本收益时再评估替换。

验收需区分数值正确与地貌真实感：除现有有限值、权重和、同机重复哈希、水/泥沙预算外，增加跨分辨率稳定坡角、碗形/凸丘曲率、河网连接/出口/湖泊、Tile 接边与重载复现；固定 Seed/相机/光照同时查看无纹理灰模、坡度/汇流图和最终材质，覆盖近/中/远景与移动 LOD。目标显卡分别测生成耗时、常驻纹理、Color/Shadow/Water GPU 帧时与峰值，而非用材质采样基准替代完整场景验收。跨厂商 GPU 的浮点逐字节一致性尚未验证。

### P16 分阶段实施与优化方案

本方案于 2026-09-30 细化，所有新增能力均为待实现设计。本文件集中维护阶段状态、优先级、设计方案和验收条件；ARCHITECTURE 只记录源码事实，README 只记录已实现行为、使用方式与验证结果。当前交付目标为连续山系、连接河谷、合理坡脚沉积、稳定近远景和可控资源成本；动态生态与植被继续由 P14 完成。

#### A：先固定尺度和数据契约

| 子步骤 | 改动范围与交付 | 验证 |
| --- | --- | --- |
| A1 基线/坐标 | 复用 Terrain Fixture，记录五预设与 512/1024/2048 数据规格；明确局部 XZ、世界单位换算、HeightScale 和正方形边界。新分层高度采用端点节点，Simulation 为单元中心，分别定义 index→position→UV，不再共用未经校正的 N/N-1 公式 | 常量面、斜面、碗形、凸丘；同一位置 CPU/GPU 采样一致；旧场景与已有 v1/v2 输出保留 |
| A2 派生/侵蚀尺度 | 新路径以 atan/坡角解释坡度，曲率由物理高度 Laplacian/间距² 得出并固定凹地为正；Talus 由稳定坡角与各方向距离换算。旧字段保留旧解释，通过显式数据/侵蚀版本进入新语义 | 三档分辨率同地貌坡角一致；曲率符号正确；坡角阈值、斜邻居、零 HeightScale、NaN/Inf 有定义 |
| A3 分层职责 | 定义 BaseHeight、DetailResidual、SimulationDelta 和每层空间范围/版本；初次先维持现有模拟分辨率，降采样作为 B4 独立变化。收敛 TerrainSettings、TerrainRuntime、Generator 与 Renderer 边界 | 资源清单、版本失效表、生成/Reset/场景复制时序完整；旧 YAML 缺字段不会自动迁移 |

新高度由 `Hrender = BaseHeight + DetailResidual + Resample(SimulationDelta)` 组成，均先换算到同一局部高度单位。SimulationDelta 是相对模拟初始高度的差，不把 Runtime 绝对高度再次相加。DetailResidual 只承载基础高度未覆盖的频带，限制坡度/幅度，河道附近由排水遮罩削弱；新层生成时不能逐 Tile 重新归一化高度，否则会破坏接边与海拔。新数据布局版本与 Noise SynthesisVersion 分开，C 阶段新地貌算法才增加合成版本；预设切换继续保留版本。

生成器独占静态 Height 和侵蚀历史场；Hydrology 独占动态 Height/Water/Sediment；Climate 独占气候场；Tile 缓存只读取并重建渲染数据。新增 CPU 数据/缓存工具放在引擎 Terrain 层，GPU 分 Pass 沿用 SimulationGrid，Inspector 仅提交配置/事务，EditorLayer 不承载生成算法。正式 ECS 组件位于 `Scene/Components.h`，不要误改旧 `Scene/TerrainComponent.h` 原型。首版物理参数按局部单位定义，非均匀 Transform 不宣称具有正确的世界物理尺度。

#### B：局部数据、几何与模拟映射分别落地

1. **B1 等价 Tile 路径**：先按世界坐标将既有高度重采样到 Tile，Detail=0，保持模拟不变；先证明范围、边界和 Color/Shadow 表面等价，再提高精度。Tile key 包含地形身份、数据/生成版本、坐标、层级；相机只改变驻留，不能进入噪声 Seed。
2. **B2 局部细节与缓存**：候选 Tile 宽 128 单位，256 格/257 个端点，采样间距 0.5；每边 2 个 Halo，共 261² 节点。共享边界按同一全局坐标计算。Halo 仅供固定半径派生核，不能假设能支持任意次数侵蚀；局部独立水蚀暂不实施。缓存按每帧请求、可见与阴影需求保护、LRU 淘汰、生成预算推进；未就绪使用粗高度并渐变接入，版本旧结果禁止发布。
3. **B3 连续几何**：候选单 Patch 64 格配合层级拆分，近处细分到能表达 Tile 高频的范围，不能只换纹理。以高度误差投影到屏幕选择 LOD，增加迟滞与父子 Morph；相邻边界统一父级约束，三角化嵌套并在全 Morph 区间保持接边。Skirt 保留为退路。同帧冻结表面描述供 Color/Shadow/Water 使用，Shadow 可以独立选精度，但要满足同一高度重建契约。
4. **B4 可选模拟降采样**：先检查 1024→512 的面积平均初始高度与水量、泥沙量转移，再创建新的全局模拟状态；初版改变分辨率要求显式 Reset，禁止运行中静默丢失水量。限制每帧派生刷新，沿用单一环境固定步，不随 Tile 加载重复推进模拟。

静态生成版本改变时失效整套 Tile 与模拟初始状态；仅 Runtime Delta 改变时刷新受影响的派生内容与帧版本，不重建 Seed 或静态细节。同帧 Prepare 在 Shadow/Color 多次调用仍只更新一次；帧开始已发布的完整资源保留到最后一个消费者结束，避免 Tile 淘汰造成悬空引用。候选低/中质量先比较 8/16 个 Tile，32 个 Tile 仅作为高质量压力档；默认档由 F 的完整帧测试选择。

水面不能直接把粗水深加到含细节的地面：先由粗地面+水深得到候选自由水面，再按细地面裁出干湿区域；按粗单元覆盖面积校正细层可见水深体积，使浅水不会沿细沟漂浮或埋入山坡。该重建为渲染派生，不回写 Hydrology；无法同时满足体积和稳定岸线时回退粗表面，B4 不通过则保留原模拟分辨率。Runtime Delta 更新需刷新同版本派生法线与 Bounds，不应重跑静态地貌/Authoring 侵蚀。

候选资源预算如下，数值为设计估算，必须在目标设备实测定标：

| 常驻资源 | 候选规格 | 原始纹理成本 |
| --- | --- | --- |
| 全局生成/派生 | 1024²，32 字节/格 | 32 MiB |
| 全局水文/气候 | 512²，116 字节/格；B4 后才启用 | 29 MiB |
| 局部缓存 | 16×261²，Height R32F + 三张 RGBA16F，共 28 字节/节点 | 约 29.1 MiB |
| 地貌历史/排水附加场预留 | 1024²，最多四张 R32F 的候选打包预算 | 16 MiB |
| 合计 | 不含 Mesh、CPU 缓存、Mip、驱动及临时资源 | 约 106.1 MiB |

B4 前水文/气候仍需 116 MiB，以上合计约 193.1 MiB；Halo、更新暂存、派生场需要完整计入，不能以 106.1 MiB 宣称整帧显存占用。候选 B4 后 Terrain 原始常驻纹理上限 192 MiB、创作临时峰值 256 MiB；加入新场必须重新列清单，超过预算优先减少 Tile 驻留/历史图分辨率并保持粗层可用，不默认全链 4096²。缓存数量只是质量档候选，不能承诺所有相机高度均足够。

#### C：山系、流域与河谷构成同一骨架

**C1 地貌控制场**：在新 SynthesisVersion 中分离区域高度、山系方向、山脊强度、岩性/硬度与盆地遮罩；先复用现有噪声核。主脊连接到支脊，山麓降低坡度和细节幅度；Plateau 用局部岩层/侵蚀抗性约束台地，Volcanic 保留世界半径和指定中心。控制场只保留被后续阶段消费的通道，避免为每个信号常驻独立 RGBA 图。

**C2 排水拓扑**：全局粗网格先用确定性的 CPU Priority-Flood 作为创作基线，显式区分可排水洼地和保留湖泊；输出出口、湖泊/溢流高度、流向和处理后的排水参考高度。初版采用 D8，等高区按洪泛顺序/稳定索引破平，禁止出现流向环；它是拓扑骨架，不直接作为最终河道折线。累计面积按格面积累加，种子、相同高度的 tie-break 与边界策略均固定。只在 Regenerate 时运行，不把 CPU 算法塞进每帧 Prepare；先测耗时，超过创作预算再评估 GPU 化。

**C3 河谷剖面**：按累计汇流面积区分主河道与支流，用经过约束平滑的路径/距离场控制宽度、切深、河床坡降和坡脚扩展；平滑后复验下游不逆坡及连通。保留指定湖泊和闭合盆地，不为“所有点流出边界”强行削平地形。河网及沟谷处理在细节之前，并对高频残差施加通道保护。每次侵蚀后重新核对排水拓扑，不能长期使用过时流向。

门槛：所有非湖泊流向链终止于指定出口或湖泊，环路数为零；面积累计与流域总面积匹配，分辨率增加时主河道世界位置基本稳定；固定灰模显示主支谷、分水岭和山麓，不能只在汇流着色里看见河网。D8 台阶是明确风险，若剖面平滑仍不能消除方向伪影，再评估多流向，避免同时重写全部链路。

#### D：有限次创作侵蚀与可重建配方

**D1 数值稳定**：先核查已有近干格速度极值，明确 Water/Flux/Velocity 单位、CellSize 与时间步关系；使用可用水出流限幅、干格阈值、时间步/子步限制，统计 dropped time。Authoring 暴雨不能直接沿用任意视觉速度限幅来掩盖模拟问题。原 CPU/GPU Contract 持续通过后才放大侵蚀。

**D2 多尺度侵蚀**：独立创作状态从 C 的高度初始化，执行有限次降雨→通量→泥沙输运→容量→侵蚀/沉积→热蚀；按硬度、覆盖物厚度和流量调制侵蚀，保留每步高度改变量与侵蚀下界。先在粗尺度形成谷地/堆积，再于有边界约束的细尺度刻画沟槽；首版不对每个渲染 Tile 单独模拟。记录侵蚀深度、沉积厚度和热蚀碎屑场，密度统一后核算地形+泥沙质量，边界出流记入预算。

**D3 持久性**：首版优先保存可重建配方：Seed、世界规格、算法版本、阶段参数、有限步数和输入资产标识；GPU 缓存仍不写 Scene YAML。固定环境重建能恢复静态侵蚀结果，旧版本有明确支持/失败语义。Inspector 参数编辑进入现有事务式 Undo；Reset 仅清 Runtime，Regenerate 显式重建静态创作结果。显式导出烘焙高度可在已有资产路径上后续完成，Runtime 模拟结果 Bake 与完整内部资产格式继续单列，不能伪装为配方保存。

#### E：地貌材质与预设表达

**E1 权重语义**：Rock 对应陡坡、硬岩与侵蚀露头；Soil 对应沉积/碎屑及低坡区；Grass 对应可生长土层与静态湿润潜力；Snow 保留显式静态雪线。真实汇流、积水与沉积控制湿润潜力，局部落差不再叫河流。先统一曲率正负语义，再生成有限、非负、归一化的四层权重；低海拔不一律湿润，陡岩也不一律有草。

**E2 表面精度/成本**：近景在片元按全局/Tile UV 读取必要的法线与地貌权重，保留几何级别坡度用于宏观分层，细节法线不反向改变岩层归属；远景使用过滤后的场和较少材质采样。新增采样用既有 Terrain Benchmark 测净增成本，在最终权重后执行 Top-2，防止“减少采样”改变归一化与层选择。微细节进入法线/粗糙度和世界坐标纹理扰动，色差频带低于颗粒频带，避免整山均匀碎纹；高度图不承担亚像素颗粒。纹理扰动启用与否由固定镜头比较重复纹理和 GPU 成本决定。

**E3 预设验证**：Alpine 验证尖脊/支谷/坡脚，Plateau 验证台地/崖壁/堆积，RollingHills 验证平缓丘陵/连续浅谷，Volcanic 验证火山尺度/径向水系，ErodedValley 验证连接主谷/支流/沉积区。使用现有五个 Seed 加两个压力 Seed，逐预设保存参数与可重建结果；HeightScale 是总幅度而非实际起伏，另报告 min/max、实际 relief 和坡度分布，禁止统一放大高度替代形态改善。

P14 后续消费静态权重与侵蚀历史，再以 Temperature、Moisture、VegetationPotential 调制生态权重，湿度/降雪不能每步重跑上述 Authoring 链。树、草、碎石实例、复杂岩壁 Mesh 和体积地形继续作为独立后续工作，不成为 P16 完成门槛。

#### F：统一数值、视觉与性能验收

| 验收项 | 候选门槛/执行方式 |
| --- | --- |
| 单位/派生 | 同一解析曲面三档分辨率，内部样点坡角误差 ≤1°；碗形/凸丘符号 100% 正确；曲率比较排除边界并以解析值和离散误差定标 |
| 接边/缓存 | 同级共享边高度误差 ≤1e-5×max(HeightScale,1)，单位法线差 ≤1e-3；跨级全 Morph 过程边位置一致；淘汰/重建不改变同坐标结果 |
| 排水 | 非湖泊路径合法终止率 100%、环路 0；闭合流域总面积误差 ≤1e-4；湖泊/出口策略有显式预期 |
| 守恒 | 新映射/侵蚀预算相对误差候选 ≤1e-4，以 max(初始量,累计绝对源项,小量下限) 归一化并同时报告绝对误差；零预算格单独验证；已有 Contract 更严的阈值保留 |
| 权重/复现 | NaN/Inf 0，负权重 0，权重和偏差 ≤0.01；同 GPU 固定版本/Seed/步骤重建一致；跨厂商比较容差统计，不要求未验证的字节哈希一致 |
| 视觉 | 同 Seed/光照/曝光，近中远灰模→坡度/曲率→汇流/沉积→完整材质；用移动相机检查接边、岸线、加载和 LOD；候选几何投影误差 ≤2 个当前视口像素，阴影和水面另检，不以颜色渐变代替几何验证 |
| 性能 | 固定实际 Viewport 与质量，预热 60 帧、采集至少 120 个有效 GPU 样本，报告中位数/p95、生成/模拟子步和 Color/Shadow/Water 分项；冷缓存飞越/重建单独统计。静止热缓存新增管理成本候选不超过等质量基线 p95 的 10%，不承诺未测的 60 FPS |
| 集成 | 每个涉及代码的阶段完成 VS2026 Debug x64 完整构建、GlimmerTests 无窗口回归、相关真实 GPU Contract；保存/重载、Undo/Redo、Edit/Play/Stop、Reset、Resize 与失败回退覆盖 |

所有阈值是实施起点，不是本次验证结果；测量需写明 GPU、驱动、场景规格、质量和模拟是否开启。普通帧不同步 Readback，不编写仅复述实现的测试；解析地形、守恒、版本往返和缓存失效是优先回归对象。复用现有本地 Fixture/诊断，不新增项目截图、日志或独立报告归档。

每阶段先取得小范围可运行结果再扩大：A 先通过解析场，B 先 Detail=0，C 先单个已知流域，D 先受控降雨，E 先单个 Alpine 灰模/材质，F 再覆盖五预设与压力场景。发生接边、守恒或兼容失败时停在该阶段，修复后再继续；性能不足时回退 Tile 数量、细节频带与材质采样，保留山系/河谷骨架。P16 总验收必须同时通过形态、稳定性与资源门槛，阶段方案本身不计作地形能力已实现。

## 后续任务

任务按依赖和建议实施顺序排列。除非用户调整方向，当前主线完成后依次提升。自动化回归可以穿插建设，但同一时刻仍只保留一个功能主线。

### P14：简化气候与植被闭环（待继续）

**依赖**：P13 已稳定，P8 能接收湿度/植被材质权重，已满足。

**状态**：待继续。CPU 基线、GPU 场、Terrain Runtime 所有权、诊断、受控 GPU Contract、水文守恒耦合、可复现视觉基线与水面渲染基础已完成；气候材质反馈与植被实例化尚未完成。

**目标**

- 使用单层二维场实现 Temperature、Evaporation、Humidity Advection、Orographic Lift、Condensation/Rainfall；
- 把降雨反馈到水流/侵蚀，把湿度和温度反馈到植被分布；
- 首版使用常量或程序化水平风，不直接实现三维流体大气。

**验收**

- 湿度能随风输运并在地形抬升区形成更多降雨；
- 蒸发、水汽、降雨和地表水变化具有可解释统计；
- 植被响应可复现，不把单株测试实体永久写入默认场景。

**已完成阶段（2026-08-19）**

- 新增纯 CPU `TerrainClimateRuntime`，Temperature 使用摄氏度，AtmosphericMoisture、Rainfall 与 SurfaceWater 使用米水当量，VegetationPotential 固定在 `[0, 1]`；
- 固定步顺序为温度松弛 → 蒸发 → 保守迎风湿度输运 → 饱和/地形抬升降雨 → 植被潜力响应，封闭边界下统计大气与地表总水量预算；
- 无窗口回归覆盖暂停/单步、风向输运、水量守恒、蒸发转移、迎风坡降雨、植被响应、Reset 与帧划分确定性；
- CPU Runtime 保持独立，不进入 Scene YAML，不创建单株植被实体，也不依赖具体模型资源。

**GPU 场阶段已完成（2026-08-19）**

- 新增 `TerrainClimateGPU`，由 `TerrainRuntime` 独占 Temperature、AtmosphericMoisture、VegetationPotential 三组 `R32F` Ping-Pong 和 Rainfall 派生纹理；
- 三段 Compute 固定执行 Temperature/Evaporation → Conservative Upwind Advection → Condensation/Orographic Rain/Vegetation Response，每段 Barrier 后才交换所有权；
- TerrainRenderer 按 GenerationVersion 重建气候 Runtime，并通过 FrameSerial 保证 Shadow 与多个 Chunk 的重复 Prepare 不会重复推进；
- DebugPanel 已提供 Play、Single Step、Reset、Wind、Initial Moisture、显式 Readback 和四种 Terrain 诊断着色；
- GTX 1050 / OpenGL 4.6 受控 `3×1` GPU Contract PASS：下风向湿度 `1`、迎风坡/平地最大降雨 `0.2/0`、帧划分差值 `0`。

**气候—水文耦合阶段已完成（2026-08-19）**

- Climate 新增逐格 Evaporation 和有符号 `WaterSource = Rainfall - Evaporation` 两张 `R32F` 输出；Climate 不写 Water，Hydrology 仍是 Water Ping-Pong 的唯一写入者；
- Hydrology 的 Flux、Update 与 Sediment Transport 统一消费空间 Source/Sink；负源项按当前可用水深限幅，Update 同步累计实际应用量，显式 Readback 由 GPU 预算场重建 Expected Water；
- 新增 `TerrainEnvironmentGPU`，统一拥有固定步累加器，每个子步严格执行 Climate → Barrier → Hydrology；两个既有 Play/Single Step 入口都进入同一耦合时钟；
- Debug Readback 同时给出 Atmospheric+Surface Total Water、Expected Total 与误差；GTX 1050 Source/Sink Contract 得到最终水深 `0.06`、预算误差 `0`，原水文相对质量误差保持 `9.83321e-7`。
- Debug Climate 暴露 `Temperature Lapse`（摄氏度/世界单位）；默认保留物理近似值 `0.0065`，小尺度 Terrain 可提高到 `0.05～0.10` 放大可视温度梯度，不需要改变 Terrain 几何比例。

**下一步**

1. **恢复 P14 后的首项：气候材质反馈**。定义静态地貌与动态生态权重组合契约，接入 Humidity、Temperature、VegetationPotential；加入湿润、植被覆盖与低温积雪，最终 Grass/Soil/Rock/Snow 有限、非负、归一化，气候步不得重复运行完整 Authoring 派生链。
2. **材质反馈验证**。覆盖相同 Seed/固定步、极端输入、非有限值、权重和、Reset、帧划分和 Terrain 重建，并补真实 GPU 截图或 Contract。
3. **GPU 植被实例化**。整理树木/灌木/草地资产与适生分布规则、确定性 Seed、Instancing/LOD/剔除/批次；测试实体仅在隔离 Lab 中，验证 Reset、分布复现与 Edit/Play 隔离。
4. **P14 总体验收**。检查风向输运与迎风坡降雨、闭合水预算、正常画面的水文/气候响应和植被复现，完成完整构建、无窗口回归、GPU 验证与三份文档同步。

### P15：统一图像导入与内部纹理资产

**依赖**：P13/P14 主线完成。若水材质、专业材质或跨格式资源需求提前成为阻塞，可在不扩散主线的前提下单独提升优先级。

**目标**

- 在引擎核心资产层建立后端无关的 `ImageDecoder` 与 `ImageData`，统一宽高、通道、像素类型、颜色空间、方向和元数据；把文件解码从 `OpenGLTexture2D`、`TextureCube` 与 `EnvironmentMapLoader` 中移出；
- 将 OpenImageIO 作为编辑器/资产导入依赖，以最小格式集合覆盖 PNG/JPEG/TGA/BMP、Radiance HDR 和 OpenEXR；首版 EXR 仅接收单 Part、非 Deep、RGB/RGBA/Y、HALF/FLOAT 的线性图像；
- 保留 `AssetRegistry` 对 `TextureColorSpace`、`TextureSemantic`、Normal DX/GL、通道映射和方向的显式契约，不使用文件元数据隐式改写材质语义；
- 建立版本化内部纹理资产及导入设置，使运行时读取烘焙结果，不要求部署 OpenImageIO 及其格式插件；
- 按 Texture2D → 等距柱状 HDR/EXR → 六面 Cubemap 的顺序迁移；完成行为一致性回归后再移除 `stb_image`。

**验收**

- `.exr` 可被识别为 Cubemap，并复用现有 TextureCube、Skybox、Irradiance、Prefilter 与 BRDF LUT 链路；`.hdr` 输出保持一致；
- PNG/JPEG/TGA/BMP 与六面 Cubemap 的尺寸、通道、翻转、颜色空间和采样结果无回归；
- 图片解码层不依赖 RendererAPI/OpenGL，`Platform/OpenGL` 不再直接调用文件解码库；
- Windows 新设备可复现构建所需的最小 OpenImageIO 依赖，并可验证版本与构建产物；
- 发布运行时可在不携带 OpenImageIO/plugin DLL 的情况下加载内部纹理资产；
- 无窗口回归覆盖 UInt8/Half/Float、1～4 通道、方向、颜色空间和 EXR 高亮值保持。

### 长期候选

- Vulkan 后端实际实现，而不只是接口预埋：先统一 RendererAPI/Buffer/VertexArray/Texture/Shader 后端创建入口，再建立 Context、Surface、Swapchain 与帧同步，以及 Render Pass/Pipeline/Descriptor/Command Buffer 映射；定义 GLSL→SPIR-V 编译、反射布局、版本化缓存与热重载失败回退。该候选尚未开始，不改变当前 OpenGL 主线；
- GPU Driven Rendering、遮挡剔除和间接绘制；
- 完整三维大气与体积流体；
- Linux/macOS 应用图标和打包资源；
- 统一 Asset Command、Dirty 状态和保存提示；
- 发布构建、资源打包与项目模板。

## 已完成里程碑

此处只记录足以影响后续决策的结果。完整设计、代码片段和教学说明位于 README。

### 2026-09-30：P16 地形系统分析与主流方案对照

- 文档职责修正：完整迁移 P16 设计分析、A～F 实施方案、候选预算和验收矩阵到本文件；README 移除方案与未来顺序，只保留实现/操作/验证与当前限制。三文档职责表明确 README 不保存未实施计划；ARCHITECTURE 仅同步职责说明，不新增计划结构。验证：迁移内容完整性、交叉引用和文档 diff 检查；无代码变化，未运行构建/GPU；提交：待提交；

- 阶段方案细化：P16-A～F 已拆为尺度/分层契约、局部采样与 Morph、山系/河网、创作侵蚀/配方、静态材质/预设和目标设备总验收；明确依赖、子步骤、候选资源预算和数值/视觉门槛。阶段实施尚未开始，未改变引擎行为；README 保留 v2 已实现行为与验证历史；
- 方案核查：模块/正式组件位置与源码一致；16×261² Tile 约 29.1 MiB，候选完整常驻纹理降采样前/后约 193.1/106.1 MiB；文档 diff 检查通过。ARCHITECTURE 已再次审查，本轮无架构事实变化，无需新增文字；未运行构建或 GPU，未将候选门槛记为实测结果；

- 完成源码审查及 Unreal Landscape、Unity Terrain Tools、Houdini HeightField 官方文档对照；具体结论与建议收录于本文件“P16 设计依据与源码审查”；本次未改变生成、渲染或模拟行为，P16 总体仍进行中，下一实施项仍为近远分层高度数据设计；
- 核实默认 1024 世界范围的高度图采样约 1 单位，LOD0 Chunk 网格采样约 2.98 单位；完整生成/水文/气候纹理按源码格式估算为 148 字节/格，1024² 约 148 MiB，4096² 约 2368 MiB，不含其它渲染资源与驱动开销；
- 新记录热侵蚀尺度语义、曲率符号契约、局部 Flow 与真实汇流区别、顶点采样派生权重等限制；建议分层高度与模拟解耦、汇流约束和可持久 Authoring 侵蚀，均为后续设计建议，未标为已实现；
- 验证：源码与资源格式静态核对、尺度/显存算术核对及文档 diff 检查；未运行构建或 GPU，未新增视觉验证结论。三份文档已审查并同步；提交：待提交。

### 2026-09-23：P16 条件分形地貌生成 v2

- Terrain Noise 新增可序列化 SynthesisVersion；新地形默认 Conditional Fractal v2，旧 YAML 缺字段时固定回退 Legacy v1，切换预设保留当前版本；Inspector 可显式比较两条路径；
- v2 分离低频陆地区域、定向山带、丘陵、高频细节和山地条件信号；Ridged/Worley/裂谷/趋势/谷地只在适用区域组合，高频 octave 根据高度图像素足迹降权，Volcanic 使用世界尺度半径；噪声核仍为现有梯度噪声；
- GPU 验证：Intel Iris Xe / OpenGL 4.6.0 上 GenerateFBM/ThermalErosion/Derive Shader 编译成功，同一 Alpine 规格重复生成哈希 6355609087535305251，高度范围 0.0814901～0.6298751、均值 0.2167487、标准差 0.1010291；Terrain Sampling Benchmark 三档各 30 样本平均 40.277/20.008/13.652 ms；
- Windows Debug x64 完整构建与无窗口回归 PASS，覆盖 Scene 往返、缺版本旧场景回退、旧版切换预设不迁移及预设相对世界尺度高度；固定 Alpine Fixture 以 558×353 完整渲染链分别导出 v1/v2，SHA-256 为 `30000B48...80027` / `BFC65B78...DC065`，两次进程正常退出且 stderr 为空；提交：待提交。

### 2026-09-22：P16 大尺度地形第一阶段

- 新建 Terrain 默认世界长宽 1024、HeightMap 1024、HeightScale 96；每轴 Chunk 数量按世界尺寸从 3 自适应到 16，1024 为 4×4、2048 为 8×8，仍共享三档网格和整张 HeightMap；
- FBM 增加可序列化世界尺度频率，新地形随 WorldSize 扩大保持相近地貌特征尺度；旧 YAML 缺字段时恢复旧 UV 模式，预设切换保留模式；
- Color LOD 按 Chunk XZ 最近距离选择并稳定相邻级差；水面按视觉水深上限保守视锥剔除；编辑器远裁剪面与移动速度适应大范围地形；
- 验证：Windows Debug x64 完整构建与无窗口回归 PASS；旧场景往返、2048 地形 8×8 覆盖与邻块 LOD 回归 PASS；Intel Iris Xe / OpenGL 4.6.0 编辑器 GPU 地形采样基准 PASS，三档各 30 样本平均 43.168/23.008/15.566 ms；git diff --check PASS。尚未完成固定相机视觉对照和目标显卡性能验收，故 P16 总体仍为当前主线；提交：待提交。

### 2026-09-21：P14 水面渲染基础

- 新增引擎侧 WaterSurfaceRenderer 与独立 Shader，Scene 收集既有 Terrain Runtime，只读 Height/Water/Velocity/Sediment；完整编辑器在 Skybox 后、Sprite/透明模型前执行，不修改模拟、不新增默认场景实体；
- 增加等尺寸 Color0/Depth GPU 快照，避免附件反馈；水面具备吸收/深度颜色、屏幕空间折射、环境反射、速度泡沫、泥沙染色，以及 Terrain 瞬时岸线湿润；写入水面深度、世界法线和 Terrain EntityID，支持 Resize、Reset、调试绕过和会话参数；
- 近干格/非有限输入和速度极值在视觉侧保护，波纹相位使用固定步模拟时间。水下透明折射、分层透射、SSR、独立水体 LOD/逐块水量剔除和持续湿润留在技术债中，不宣称 P14 总体完成；
- 验证：VS2026 Debug x64 完整构建与无窗口回归通过；Intel Iris Xe / OpenGL 4.6 本地 GPU 契约覆盖干格、吸收/折射/泥沙变化、前景遮挡、EntityID/Normal、极端/NaN/Inf、Resize、复制绑定隔离、重叠水面顺序、关闭/诊断绕过及 Reset，Water 输入绘制前后相同；完整编辑器隔离 Terrain Fixture 正常退出，原水文与气候 Contract PASS；
- 验证程序/日志仅留本地 bin；三份现有文档同步，无新增验证脚本、报告或截图归档。下一实施项为动态生态材质权重；提交：待提交。

### 2026-09-21：P14 Terrain 本地视觉基线验证

- 本地固定 Alpine Seed 11、1280×720、相机/光照和环境参数，Reset 后执行 120 个耦合单步；Intel Iris Xe / OpenGL 4.6.0（32.0.101.6790）两次验证的九张图像 SHA-256 和模拟统计一致；
- 初始图与第 120 步正常图逐字节相同，确认正常材质尚未反映气候/水文状态；正常视图 30 样本平均分别为 131.041/145.978 ms，9 Draw、33326 三角形，性能波动不作为优化结论；发现最大速度 44320，保留在技术债中；
- 新增只读 Water Velocity 调试视图。按用户要求，移除专用采集脚本、自动运行类、截图读回接口、独立报告和项目内截图/日志归档，仅保留本地验证结果与现有文档中的必要结论；
- 验证：基线阶段 VS2026 `Debug | x64` 完整构建、全部无窗口回归和两次 GPU 采集通过；清理后重新生成工程，完整构建、无窗口回归、无残留引用检查及 `git diff --check` 通过。P14 总体未完成，下一实施项仍为 Water Surface Pass；提交：待提交。

### 2026-09-17：Inspector 跨对象编辑状态隔离

- Inspector 现在分别以实体稳定 UUID 和 AssetHandle 作为整组 ImGui 控件的 ID 作用域；切换实体或资产时，新对象的同名控件不会继承上一对象仍处于活动状态的编辑缓冲；
- Inspector 调整到选择来源之前绘制，使当前控件先完成失焦和连续编辑事务，再由 Hierarchy 或 Content Browser 切换选择；修复了编辑实体名称时直接点击另一实体，旧名称同时写入两个实体的问题，并防止 Transform、组件属性及共享材质滑块出现同类跨对象写入或事务丢失；
- 审计确认列表型界面已有各自作用域：Content Browser 条目、Shader 列表、Terrain Material Layer、材质纹理槽和后处理 Pass 均在循环内使用独立 ID；未改变 Scene、SelectionContext 或组件序列化边界；
- 验证：`scripts\Verify-Windows.bat` 通过 VS2026 `Debug | x64` 完整解决方案构建和全部无窗口回归；提交：待提交。

### 2026-09-14：Viewport Gizmo 变换撤销

- Viewport Gizmo 的移动、旋转和缩放已接入 `EditorCommandHistory`：开始拖动时保存 Scene、实体 UUID 和完整 Transform，拖动期间实时预览，释放时只提交一条已执行命令；无实际变化时不产生空命令；
- `Ctrl+Z` 可恢复 Gizmo 拖动前的位移、旋转和缩放，`Ctrl+Y`/`Ctrl+Shift+Z` 可重做；场景切换会清理未完成的 Gizmo 事务，命令通过 UUID 查找目标而不依赖临时 EnTT Handle；
- 验证：`scripts\Verify-Windows.bat` 通过 VS2026 `Debug | x64` 完整解决方案构建和全部无窗口回归；提交：待提交。

### 2026-09-14：后处理 Normal、Velocity 与 History ABI

- Scene FBO 新增带逐像素有效标记的世界空间 Normal MRT；PBR、Toon、Terrain、Sprite 与 Skybox 明确写入或清空该附件；Framebuffer 新增 `RG16F` 和浮点颜色附件定点清理能力；
- PostProcessRenderer 新增由 Depth + Current Inverse VP + Previous VP 生成的全分辨率 Camera Velocity，以及两张全分辨率 `RGBA16F` Custom History Ping-Pong；History 在 Resize、场景/模式切换、调试场景切换、Pass 结构变化和相机聚焦时失效；
- `PostProcessABI` 新增 `SceneNormal/Validity`、`Velocity`、`HistoryColor/Valid`、能力标记及对应邻域采样函数；新增 NormalOutline、CameraMotionBlur、TemporalEcho 三个示例，资产创建模板同步说明新输入；
- 当前 Velocity 只覆盖静态世界在相机运动下的屏幕位移，尚无动态 Model/Instancing/Sprite Previous Transform；History 也尚无 Depth Disocclusion、Neighborhood Clamp、Reactive Mask 或投影 Jitter，因此不能视作完整 TAA；
- 验证：`scripts\Verify-Windows.bat` 通过 VS2026 `Debug | x64` 完整构建与全部无窗口回归；GTX 1050/OpenGL 4.6 成功编译内部 Velocity/History Shader、PBR/Toon/Terrain Normal 输出及九个示例 Pass，并确认 Normal、Velocity、History 在五帧真实链路中有效；提交：待提交。

### 2026-09-14：编辑器场景保存可靠性

- `SceneSerializer` 新增内存快照输出；EditorLayer 以实际可序列化内容而非 CommandHistory 推断 Dirty，因此 Tag、Sprite、Model 等尚未完全进入命令栈的直接组件修改同样不会漏报；原生窗口标题显示 `场景名* - Glimmer Editor - Cyou Branch`，保存或恢复干净状态后立即移除星号，File 菜单保持普通 `Save`；
- New、Open、内容浏览器双击、Viewport 场景拖放、File/快捷键退出及原生窗口关闭统一经过未保存确认，可选择 Save、Discard 或 Cancel；保存失败时继续保留确认框并显示可见错误，不执行待定的场景切换或退出；
- 场景保存改为同目录 `.tmp` 写入与校验，再以 `.bak` 保护原文件并替换目标；失败会尝试恢复旧文件，加载时若发现替换中断造成目标缺失，也会自动恢复最后有效备份；Save As 自动补齐 `.glimmer` 扩展名，只有成功写入后才清除 Dirty、更新当前路径与启动恢复偏好；
- Application 将 WindowClose 先交给 Layer，未被编辑器延期时才停止主循环；自动验证与明确的内部关闭仍可直接结束，不受用户场景弹窗影响；
- 验证：`scripts\Verify-Windows.bat` 通过 VS2026 `Debug | x64` 完整解决方案构建和全部无窗口回归；新增内存快照直接组件变更检测、已有文件替换、中断备份恢复与 `.tmp/.bak` 清理断言；GTX 1050/OpenGL 4.6 上独立 Terrain Fixture 完成真实首帧、Shader/Compute 加载和三档基准后正常退出；Fixture 退出明确跳过“上次场景”偏好写入，不污染用户恢复目标；提交：待提交。

### 2026-09-14：Terrain 世界长宽与网格精度解耦

- `TerrainSpecification` 新增独立、可序列化的 `WorldSize`，Inspector 以 `World Size (X/Z)` 暴露正方形地形的水平长宽；范围从 16 到 8192，默认 256；
- `WorldSize` 统一驱动程序化生成的物理尺度、派生图采样间距、水文/气候格点尺度、`3×3` Chunk 布局、Color/Shadow Bounds 和编辑器聚焦范围；`MeshResolution` 继续只决定共享网格细分，最大值仍为 512，放大地形不再隐式平方增加三角形数量；
- 旧场景没有 `WorldSize` 时从原 `MeshResolution` 推导，保持其既有水平尺寸；新场景保存时写入经过有限值与范围约束的字段；
- 验证：`scripts\Verify-Windows.bat` 完整通过 VS2026 `Debug | x64` 解决方案构建与全部无窗口回归；新增世界尺寸序列化往返、上限和非有限值回退断言；`git diff --check` 通过；提交：待提交。

### 2026-09-14：第三方生成残留清理与 tmpTerrain 方案复核

- 删除 GLFW、ImGui、yaml-cpp、ImGuizmo 和 SPIRV-Cross 子模块内遗留的未跟踪 `premake5.lua`、Visual Studio 工程及局部 `bin/bin-int`；未重置或修改第三方源码，根工作树恢复干净；
- 复核 `tmp/tmpTerrain` 全部 16 个 HLSL 文件：原型缺少 Common、ShadingModels、Random、Meteorograph、WorldDefinitions、MeshGeneration 等外部依赖，固定使用 4096² 数据场与 256² 相机跟随网格，不能作为当前 OpenGL 引擎的可直接替换模块；其 `TerrainData_CS` 在同一 Dispatch 中写 Flux 后仅用 Workgroup Barrier 读取跨组邻格，无法保证全局可见性，泥沙半拉格朗日输运也不满足当前守恒契约；
- 决策：不整体采用原型运行时；保留现有可序列化 Terrain、Chunk/LOD、Triplanar PBR、CSM、确定性派生和分 Pass 守恒模拟，选择性重写原型更有表现力的独立水面、折射/深度吸收、流速泡沫、泥沙染色以及海岸/湿润/积雪生态反馈；本次只完成分析和路线校准，尚未实现视觉 Pass；
- 验证：清理后五个子模块及根工作树均无残留变化；重新执行 `scripts\Verify-Windows.bat -SkipBuild`，中央适配层成功生成 VS2026 工程且没有重新污染子模块，全部无窗口回归 PASS；`git diff --check` 通过；
- 文档修正：移除已经由中央依赖适配层解决的子模块 Premake 技术债，修正 P14 气候—水文耦合状态，并为可核验里程碑回填真实提交；提交：待提交。

### 2026-09-11：纯净 Clone 的 Windows 构建自举

- 定位新目录构建失败的根因：GLFW、ImGui、yaml-cpp、ImGuizmo 与 SPIRV-Cross 的 Glimmer 专用 `premake5.lua` 只存在于旧工作树的子模块未跟踪文件中，上游固定提交并不包含它们；纯净 clone 即使完成 `git submodule update --init --recursive`，根 Premake 仍无法包含这些本机文件；
- 新增主仓库受控的 `scripts/premake/Dependencies.lua`，集中定义 GLFW、Glad、ImGui、yaml-cpp、ImGuizmo 与 SPIRV-Cross 静态库项目；子模块仅提供源码，生成的依赖 `.vcxproj` 统一进入被忽略的 `bin-int/projects/Dependencies`，不再把构建适配层或工程文件写入第三方工作树；删除 Glad 目录中的旧适配脚本，根 Premake 只保留一个依赖适配入口；
- `Verify-Windows` 在生成前检查十个递归子模块的 `.git` 与代表性源码，并检查根 Premake、主仓库依赖适配层、Glad 源码和内置 Premake；缺失时给出“初始化子模块”或“恢复主仓库文件”的区分诊断；BAT 优先使用 PowerShell 7、回退 Windows PowerShell；MSBuild 子进程只保留一个规范化 `PATH`；
- README 开头新增新设备构建与启动项目指引；根工作区默认启动项目由示例 `Sandbox` 调整为主要开发宿主 `GlimmerEditor-CyouBranch`，并说明 Sandbox、旧编辑器和无窗口回归目标的用途；旧 `tmp` 第三方 Premake 备份已移除，不影响仍保留作算法参考的 `tmp/tmpTerrain`；
- 新设备验证默认使用单节点 MSBuild，避开 VS2026 多节点重新传播 `PATH/Path` 时的 MSB6001；可通过 `-BuildJobs <1..32>` 显式提高并行度；P14 当前主线保持不变；
- 验证：从根适配层重新生成 `GlimmerEngine.slnx`，解决方案引用 `bin-int/projects/Dependencies` 下六个依赖工程且不引用子模块内工程，并将 `GlimmerEditor-CyouBranch` 标记为 `DefaultStartup`；PowerShell 7 的完整 `scripts\Verify-Windows.bat` 与规范化环境下的 Windows PowerShell 5.1 回退路径均返回 0，`Debug | x64` 完整解决方案成功构建；删除备份并调整默认项目后再次执行生成与全部无窗口回归，结果 PASS；提交：`21d8c34`。

### 2026-09-10：自定义后处理 Shader ABI 与实时 Pass 栈

- 按用户显式调整的优先级，在 P14 中途插入并完成自定义后处理基础能力；新增 `PostProcessVertexABI.glslinc` 与 `PostProcessABI.glslinc`，统一全屏顶点、HDR Scene Color、Scene Depth、Viewport Resolution、Time、Camera Position 与 Inverse ViewProjection，并提供场景采样、深度采样和世界位置重建辅助函数；用户 Shader 只需实现 `GlimmerPostProcess()`；
- `PostProcessRenderer` 新增可启用、排序、移除的自定义 Pass 列表，在 Scene HDR Color 之后、Bloom/Tone Mapping 之前用两张全分辨率 `RGBA16F` Framebuffer Ping-Pong；资源按首个 Pass 按需创建，在最后一个 Pass 移除时释放，内置 Bloom/Tone Mapping 也改用实际目标分辨率而非窗口尺寸；
- Settings 新增自定义后处理列表和 `.glsl` 拖放入口，Content Browser 新增 Post Process Shader 创建模板；自定义 Shader 注册进共享 `ShaderLibrary`，继续支持主文件和递归 Include 热重载；`PostProcess` 示例集现包含 Pixelate、Vignette、Chromatic Aberration、Wave Distortion、Depth Outline 与 Film Grain；
- 修正尖括号 Shader Include 根目录：位于任意 `assets/shaders` 子目录的 Shader 均能通过 `<Glimmer/...>` 引用稳定 ABI；新增 `GLIMMER_POST_PROCESS_VALIDATE=1` 自动验证入口；
- 当时的 Pass 栈属于编辑器会话运行时状态，不写入 Scene YAML，尚无参数反射、History/Velocity/Normal 输入、阶段选择或 Render Graph；Normal、Camera Velocity 与 Custom History 已于 2026-09-14 补齐，其余边界保持不变；P14 当前主线保持不变；
- 验证：VS2026 `Debug | x64` 编辑器与回归工程构建成功，无窗口回归全部 PASS；GTX 1050 / OpenGL 4.6 下六个示例与递归 ABI Include 均编译成功，六级自定义 Pass 链连续渲染 5 帧后正常自动退出；`git diff --check` 通过；提交：`d3c7abf`。

### 2026-09-09：Content Browser 连续缩放与紧凑列表

- Content Browser 右栏新增贴近下边框的紧凑缩放滑块，并支持文件区域内 `Ctrl + 鼠标滚轮` 调整；外层 FilePanel 禁止滚动，只保留文件区自身的滚动条；文件区高度由滑块顶部直接计算，滚动条下端与滑块相接，滑块只保留 2 像素底部间距；缩放最小端切换为与内容区同宽的单行紧凑列表，其余区间映射为 `72～160` 像素自适应方形网格；
- 网格条目改为上方大图标、底部单行文件名，名称按实际像素宽度省略且悬停显示全文；原有选择、双击、拖放和空白右键创建行为保持不变；
- 文件夹与 Shader、Model、Image、Scene、Material、TerrainMaterial、Skybox、普通文件图标改由 ImGui DrawList 以几何图元绘制，不依赖字符字号或新增位图资源；当前目录按文件夹优先、名称不区分大小写稳定排列；
- 验证：VS2026 `Debug | x64` 完整解决方案构建成功，全部无窗口回归 PASS；源码兼容当前 ImGui 1.92.7，只有既有 `Instrumentor.h` C4267 警告；提交：`35235cf`。
- 当前主线保持 P14 气候场驱动 Terrain Material Weight。

### 2026-09-09：逐场景编辑器观察相机恢复与可靠卸载

- EditorCamera 新增有限值校验的 GetState/SetState，以 FocalPoint、Distance、Pitch、Yaw 作为唯一持久化状态，位置、矩阵和视口投影仍在运行时派生；
- 用户偏好升级为向后兼容的 Version 2，按规范化场景路径保存最多 64 组观察状态；场景切换、Save/Save As 和正常退出保存视角，加载场景后恢复，New 使用默认视角但不删除历史；
- Debug 临时场景进入时快照正式观察相机，退出时恢复，激活期间不写偏好；Terrain 自动验证继续使用独立 Fixture，不污染用户记录；
- 修复 Layer 生命周期：Application 在 Renderer Shutdown 前逆序、幂等调用全部 OnDetach，使编辑器会话与 GPU 资源清理拥有可靠退出点；
- 验证：VS2026 `Debug | x64` 完整解决方案构建成功；无窗口回归全部 PASS，新增逐场景隔离、非法值、范围约束、Version 1 兼容和 Layer 卸载顺序断言；同步修正架构文档中遗留的默认演示场景描述；提交：`ef6bde2`。
- 当前主线恢复 P14 气候场驱动 Terrain Material Weight。

### 2026-09-09：启动恢复上次场景与默认演示场景解耦

- 用户明确插入编辑器工作流任务后，EditorLayer 统一 New/Open/Save/Save As、内容浏览器双击和 Viewport 场景拖放入口，并持有当前 `.glimmer` 路径；Ctrl+S 直接保存已有路径，Ctrl+Shift+S 执行 Save As；
- 新增项目隔离的用户级场景偏好，成功打开/保存后立即记录，启动时尝试恢复；首次启动、New、文件丢失或反序列化失败使用空场景，不隐式保存未保存内容；
- 普通启动移除硬编码的 Sun、Point Light、Sky Light 和 Alpine Terrain；Terrain Sampling/LOD 自动验证改用独立 Fixture，不再依赖正常启动场景；SceneSerializer 现返回写入成功/失败；
- 验证：VS2026 `Debug | x64` 完整解决方案构建成功；无窗口回归全部 PASS，新增写入失败反馈、带空格路径、项目隔离和清除恢复目标断言；统一脚本仍受宿主重复 `PATH/Path` 影响，使用规范化子进程环境后同一解决方案构建通过；
- 当前主线恢复 P14 气候场驱动 Terrain Material Weight；提交：`0c81e69`。

### 2026-09-09：模型 Shader ABI 与多 Pass 法线外扩

- 用户明确调整优先级后，从 PBRModel 中抽出 Model Vertex、Forward Fragment、Surface 与 CSM 公共 GLSL ABI；图形 Shader 支持递归 `#include`、循环检测和 Include 依赖热重载，PBR 与 Toon 共用相机、实例、灯光、材质、阴影、IBL、Alpha 和 EntityID 契约；
- `.glmat` 新增向后兼容的可选 Passes，每个 Pass 保存 Shader、Order、Cull、DepthWrite、Queue 以及 Float/Float4 参数；Renderer3D 将 Mesh 展开为有序 Pass RenderItem，并把 Pass 状态纳入排序、合批、实例化和状态恢复；RendererAPI/OpenGL 新增 None/Back/Front Cull；
- 新增 `ToonSurface.glsl`、`ToonOutline.glsl` 和 `DefaultToonOutline.glmat`。Outline 以世界空间法线外扩、Front Cull 和独立不透明队列绘制，Forward 继续消费标准 Surface/Light/Shadow ABI；旧单 Shader 材质行为不变；
- 验证：VS2026 `Debug | x64` 的 Glimmer、编辑器和回归目标均构建成功；无窗口回归全部 PASS，并新增多 Pass 顺序、状态与通用参数保存往返；GTX 1050/OpenGL 4.6 下 PBR/Toon Include 展开编译成功，Toon 自动 Lab 实际渲染 `12/12` Items、无跳过模型，Shadow `24/24`；统一脚本首次受宿主重复 `PATH/Path` 环境变量影响只返回 MSBuild 退出码，清理子进程环境后相同工程构建通过；
- 当前主线恢复 P14 气候场驱动 Terrain Material Weight；提交：`c80489f`。

### 2026-08-18：P13C 运行时侵蚀、沉积与派生图闭环

- CPU/GPU 根据 Sediment 与 Capacity 差异在 Height 与 Sediment 间交换等效质量；侵蚀/沉积率默认关闭，并受 Terrain Density、可侵蚀深度和单步 Height 上限约束；
- GPU 新增独立 Runtime Height Ping-Pong 与 `ErosionDeposition.comp`；生成 Height 保持不可变初态，Reset 恢复全部模拟状态，Runtime 不进入 Scene YAML 且没有隐式 Bake；
- Terrain Color/Shadow 统一读取 Runtime Height；每帧全部固定子步结束后最多复用一次 `DeriveTerrainMaps.comp`，同步刷新 Normal/Slope、Analysis 和 MaterialWeight，零步进或源项关闭时不增加派生 Dispatch；
- 验证：114 项无窗口断言全部 PASS；GTX 1050 / OpenGL 4.6 水文 Contract 的组合质量误差 `2.58287e-7`、Height/Sediment 帧划分差值为 `0`、Reset PASS；Terrain GPU 验证通过并实际调用 Runtime Height 派生入口，哈希与生成路径一致；VS2026 `Debug | x64` 增量构建成功；
- P13C 验收完成，当前主线提升为 P14 简化气候与植被闭环；提交：`9294359`、`8b70a29`。

### 2026-08-18：P13B 泥沙输运与携沙能力契约

- CPU/GPU 新增独立 Sediment 守恒输运；Sediment 使用单位面积悬浮质量，沿 Water Flux 迁移并按可用质量限幅，Height 在本阶段始终只读；
- 新增只读 Capacity/Saturation 派生场：`Capacity = CapacityScale × WaterDepth × Speed`，`Saturation = Sediment / Capacity`，零容量但存在泥沙时以 `1000` 有界表示过饱和；
- GPU 使用单缓冲 `R32F` Capacity/Saturation 与独立 `SedimentCapacity.comp`，不增加派生场 Ping-Pong；Terrain slot 23～27 和 Debug 五态可视化提供 Water、Velocity、Sediment、Capacity、Saturation 诊断；
- 验证：109 项无窗口断言全部 PASS；GTX 1050 / OpenGL 4.6 GPU Contract PASS，水量相对误差 `7.38228e-7`、泥沙质量误差 `0`、`capacityMax=0.0411786`、`saturationMax=1000`，Water/Sediment/Capacity/Saturation 两种帧划分差值均为 `0`；VS2026 `Debug | x64` 编辑器增量构建成功；
- P13B 验收完成，当前主线提升为 P13C 运行时侵蚀与沉积；提交：`0f667af`、`8e8a091`。

### 2026-08-18：P13A 固定步长水流核心

- 建立独立于 Authoring Erosion 和 Scene YAML 的 CPU `TerrainHydrologyRuntime` 与 GPU `TerrainHydrologyGPU`；Height 只读，Water、四向 Flux、二维 Velocity 使用独立状态和固定步长调度；
- GPU 每步由 HydrologyFlux 与 HydrologyUpdate 两次 Dispatch、全局 Barrier 和 Ping-Pong Swap 组成；TerrainRenderer 仅在 Color 帧首次 Prepare 推进，Debug 提供 Play/Pause、Single Step、Reset、Rainfall、Readback 与水深诊断覆盖；
- 新增受控 GPU 合约验证：在 `3×1` 盆地中分别以 `0.04×25` 和 `0.01×100` 运行相同 100 步，自动检查有限性、`2e-3` 相对质量误差上限、低地汇聚与帧划分确定性；可从 Debug 按钮或 `GLIMMER_HYDROLOGY_VALIDATE=1` 启动；
- 验证：GTX 1050 / OpenGL 4.6 实测 PASS，相对质量误差 `7.38228e-7`，盆地水深 `0.599998`、两侧最大水深 `6.28643e-7`，帧划分最大差值 `0`；VS2026 `Debug | x64` 编辑器增量构建成功，104 项无窗口回归全部 PASS；
- P13A 验收完成，当前主线提升为 P13B 泥沙输运；提交：`3cffa91`、`91b6662`、`de662cd`。

### 2026-08-17：Assimp 跨设备导入收口

- Ensure 从“只看三个文件”升级为校验产物、Schema、配置、Assimp 子模块提交和 `ccache=OFF` 的构建指纹；Build 限定 VS 18.x/v145，PreBuild 改用 `$(ProjectDir)`，完整解决方案与单项目构建均能定位脚本；
- Cerberus 回归改用版本化 assets，明确验证静态网格、切线和 A/N/M/R，不再依赖本机 `tmp` 或声称存在未提交的 AO；IBL Gun 移除错误 AO 覆盖和失效 Raw Normal 句柄，Albedo 注册恢复 sRGB/Color；
- 定位回归 EXE 的 `0xC0000005` 为损坏 Debug 增量链接产物：旧 EXE 的 PE 系统导入表为空。测试目标关闭增量链接并使用 Program Database，定向 Rebuild 后系统导入恢复；
- 验证：旧 `ccache=ON` Cache 被自动重配，首次 Ensure 约 5.95 秒、后续命中约 118 ms；测试工程可独立构建，Cerberus 正式样本与共 104 项无窗口断言全部 PASS；`Verify-Windows.ps1` 完整通过，编辑器保持运行 10 秒无提前退出，`git diff --check` 通过；当前主线仍为 P13A；提交：`de0cad0`。

### 2026-08-16：Assimp 新设备构建自修复

- 定位新设备的 `assimp/config.h` C1083：Assimp 子模块只提供 `config.h.in`，实际 `config.h` 必须由上游 CMake 写入被忽略的 `vendor/assimp-build/<配置>/include`；仓库已有 Premake include/link 配置，但新设备未生成独立构建目录；
- `Win-BuildAssimp-vs2026.bat` 改用 `vswhere` 定位 VS 18/2026 和随 VS 安装的 CMake，移除固定 C 盘路径，并显式关闭会错误包裹 MSVC `lib.exe`、造成“日志成功但静态库未落盘”的 MinGW ccache；
- 新增快速 `Win-EnsureAssimp-vs2026.bat`，Premake 为 Debug 与 Release/Dist 写入对应 PreBuildEvent；后续由 2026-08-17 里程碑补充构建指纹、子模块提交与 CMake 配置校验。`Verify-Windows.ps1` 现检查 Assimp 子模块并在 Debug 构建前补齐依赖；
- 验证：新建 Debug 构建目录后生成 `config.h`、`assimp-vc145-mtd.lib` 与 `zlibstaticd.lib`；快速 Ensure 命中后不重复编译；重新生成 VS2026 工程，GlimmerEngine `Debug | x64` 整解决方案成功构建，AssimpModelImporter 编译和编辑器最终链接通过；当前主线仍为 P13A；提交：`113898a`。

### 2026-08-14：静态 FBX 与导入 PBR 材质

- 新增私有 `AssimpModelImporter` 并把 `.fbx` 注册为 Model；静态导入执行三角化、顶点合并、法线/切线补全、缓存局部性优化和节点变换烘焙，只输出既有纯 CPU MeshSource，Assimp 类型未泄漏到 Scene 或 Renderer；
- MeshMaterialSource 扩展 BaseColor、Normal、Metallic、Roughness、AO、Emissive 路径与因子；除读取 Assimp 材质语义外，可在模型相邻、Textures、Textures/Raw 内按 `_N/_M/_R/_AO` 约定补全外部贴图。Model 按 MaterialIndex 共享加载纹理，Renderer3D 让显式 `.glmat` 通道优先、导入通道回退；
- PBRModel 新增 Metallic/Roughness 贴图采样，固定使用 texture unit 11/12，保持 0～3 材质、4～7 CSM、8～10 IBL 的现有槽位边界；Content Browser 将 FBX 显示为 Model；
- 验证：Cerberus FBX 成功产生有效三角 Submesh，全部顶点切线有限且归一化；当时的本机 A/N/M/R/AO 验证已由 2026-08-17 版本化 A/N/M/R 回归取代。Cerberus 大文件后来已进入正式 assets，许可风险见技术债；
- 当前主线返回 P13A GPU 水文数值与视觉验收；提交：`0261eb7`。

### 2026-08-14：模型导入边界与 Assimp 接入准备

- 补齐官方 `assimp/assimp` Git 子模块并固定到 `v6.0.5`（`392a658`）；新增 VS2026 独立构建脚本，通过 Assimp 官方 CMake、NMake、静态 CRT 和 import-only 配置只构建 OBJ/FBX/glTF importer，生成目录不进入版本控制，也不加入普通 Glimmer 源文件编译；
- 新增纯 CPU `MeshSource`、`SubmeshSource`、`MeshMaterialSource` 和统一 `ModelImporter` 分发边界；既有 tinyobjloader 逻辑迁入 `ObjModelImporter`，`Model` 只消费导入结果并创建 GPU `Mesh`，为后续 Assimp importer 和内部 `.glmesh` 烘焙隔离第三方格式；
- 当前资产注册仍只把 `.obj` 认作 Model，Assimp 静态库尚未链接到运行时，FBX/glTF/GLB 仍明确不属于已支持格式；下一阶段需实现 `AssimpModelImporter`、坐标/单位/节点/材质契约和内部二进制资产后再开放扩展名；
- 验证：Assimp v6.0.5 Debug 静态库在 VS2026 下成功构建，实际输出 `assimp-vc145-mtd.lib` 并只启用 OBJ/FBX/GLTF，立即重复脚本只做约 5 秒配置/目标检查而未重编源文件；新增 3 条 OBJ→MeshSource 回归后 100 项无窗口断言全部 PASS，GlimmerEditor-CyouBranch `Debug | x64` 构建成功，立即重复构建仅检查并输出既有目标；
- 提交：`ede3d86`。

### 2026-08-13：P12.1 后处理职责收拢

- 新增引擎侧 `PostProcessRenderer`，集中持有 Display FBO、半分辨率 Bloom Ping-Pong FBO、三张后处理 Shader 引用、运行时参数以及 Bloom/Tone Mapping Pass 执行；
- `EditorLayer` 不再实现 Bloom 与 Tone Mapping 算法，只提交 Scene Color/Depth、相机逆 VP/位置、SkyLight 和 DirectionalLight 输入，并将 `PostProcessSettings` 暴露给既有 Settings UI；画面算法、默认参数和 Scene YAML 边界保持不变；
- 验证：Premake 已将新增源文件纳入 Glimmer 工程；VS2026 `Debug | x64` 整解决方案构建成功，88 项无窗口回归全部 PASS；最终编辑器以项目工作目录和雾验证开关持续运行 15 秒，无提前退出；
- 提交：`822a921`。

### 2026-08-13：P12 山脉大气表现与后处理

- Scene Depth 重建世界位置，在单一 ToneMapping 链中实现距离雾、沿相机射线解析积分的指数高度雾，以及 Manual/SkyLight/Directional Light 三档线性雾色；
- 显示映射改为 `2^EV` 摄影曝光、可调 ACES White Point 和单次 Gamma；新增半分辨率 HDR Bloom，使用软阈值提取与双缓冲高斯模糊，合成后再统一经过雾、EV 和 ACES；
- TAA 评估结论仍为暂缓：后处理现已有 Previous ViewProjection、Depth 重投影 Camera Velocity、Custom HDR History 和基础失效规则，但移动 Model/Instancing/Sprite/透明物体仍无自身速度。后续仍须补齐投影 Jitter、实体 Previous Transform、Depth Disocclusion、邻域 Clamp 与透明/发光 Reactive Mask，EntityID 保持非时域拾取；
- 验证：VS2026 `Debug | x64` 整解决方案构建成功，88 项无窗口回归全部 PASS；Intel Iris Xe / OpenGL 4.6 下全部后处理、Terrain 与 Shadow Shader 编译成功；固定相机验证远景雾化、高处细节、环境色关联、默认 EV/ACES 高光和太阳 Bloom，无断言、崩溃或整屏泛白；
- 提交：`91505b2`、`1e73acd`、`0930a54`、`8a9cbaa`。

### 2026-08-13：P11 Terrain Chunk、LOD 与剔除

- Terrain 固定拆分为 `3×3` Chunk，Color/Shadow Pass 使用共享布局与保守八角点视锥剔除；九块共用 Height、派生图和 TerrainMaterial 绑定；
- Runtime 持有约 `1 / 1/2 / 1/4` 密度的三份共享网格，Color Pass 按世界距离选择 LOD，加入 5 单位迟滞和相邻四方向最多一级的约束；Shadow 固定使用 LOD0；
- 三档网格四边使用 Skirt 遮盖 T-Junction；Debug Overview 提供 LOD 数量、三角形数、距离阈值及红/绿/蓝 LOD 调试着色；
- 验证：88 项无窗口回归全部 PASS，VS2026 `Debug | x64` 整解决方案构建成功；Intel Iris Xe / OpenGL 4.6 下 Terrain、ShadowDepth 与三条 Compute Shader 均正常加载；固定相机截图呈现连续的红/绿/蓝近中远区域，颜色边界未露出天空盒或 Clear Color，也未出现孤立越级 Chunk；
- 提交：`dccd0b7`、`e610d73`、`af2823b`、`4ba3081`。

### 2026-08-12：P10 IBL 环境光照

- 完成 Radiance HDR/等距柱状图导入、线性 `RGBA16F` Cubemap、完整普通 Mip Chain，以及 Model/Terrain 共用 SkyLight 数据链；
- `EnvironmentLighting` 按源 Handle、Runtime Version、派生图类型与生成参数缓存 `32×32 / 64 samples` Diffuse Irradiance 和 `64×64 / 64 samples`、7 层 GGX Specular Prefilter，正常帧不重复读回或卷积；
- 新增与具体环境无关、进程级共享的 `64×64 RG16F / 128 samples` Split-Sum BRDF LUT；PBRModel 使用 slot 8/9/10，Terrain 使用 slot 20/21/22，并按 `Prefilter × (F0 × scale + bias)` 完成环境镜面项；
- GTX 1050 / OpenGL 4.6 下 BRDF LUT 在日志相邻秒内生成一次；Diffuse 与 Specular 各生成一次，PBR Lab 6/6，默认 Terrain、Shadow 与三条 Terrain Compute Shader 均成功加载；
- 验证：编辑器增量构建成功，78 项无窗口回归全部 PASS，新增覆盖 LUT 有限/有界及 Roughness/掠射角响应；构建产物保留；提交：`85ae9ee`、`6a9bb58`、`d31fb00`。

### 2026-08-11：P8.1 TerrainMaterial 采样优化

- Terrain Fragment Shader 在最终高度/坡度/曲率/湿度权重确定后只保留贡献最高的两层，再执行 Triplanar Albedo/Normal/AO 采样；高性能档进一步只为主导层采样 Normal/AO，Auto Distance 在近景 Top-2 完整细节与远景主层细节之间使用 30% 宽度的 smoothstep 过渡；
- TerrainRenderer 新增独立非阻塞 GPU Timer、采样模式、距离阈值、DrawCall/绑定纹理统计；Debug Overview 可即时切换 Full-4、Top-2、Top-2 + Dominant Normal/AO 与 Auto Distance。Auto 成为默认，Full-4 与 Handle=0 的无纹理基础颜色路径继续作为质量和性能基线；
- 新增独立 TerrainSamplingBenchmarkTool：不持有 Scene 或 EditorLayer 状态，按唯一 GPU Query 样本执行 15 次预热和每档 30 次采样；无人值守入口只为默认 Terrain 分配完整 DefaultTerrain、固定相机，完成后正常退出；
- GTX 1050/OpenGL 4.6 固定场景结果：Full-4 10.681 ms，Top-2 6.026 ms（降低 43.6%），Top-2 + Dominant Normal/AO 4.226 ms（降低 60.4%）；三档固定视口截图未发现新增条带接缝，Terrain 图形/Compute Shader 均成功编译；
- 验证：Premake VS2026 重新生成成功，VS2026 Debug x64 全解决方案构建成功，64 项无窗口断言全部 PASS；提交：`5a6e815`。

### 2026-08-11：tmpTerrain 地质地貌迁移实验

- 从 `tmp/tmpTerrain/TerrainHeightInitializer_CS.hlsl` 选择性迁移 Worley 地质块、陡峭区遮罩、狭长裂谷和大尺度趋势思想；算法已重写为现有 OpenGL GLSL Compute 分支，没有引入原型的 HLSL、固定 4096² 网格或 GPU 网格初始化；
- `TerrainNoiseSettings` 新增 GeologyBlend、GeologyScale、RiftStrength 与 TrendStrength；参数进入 Terrain Preset、Scene YAML、Inspector 连续编辑事务和 Runtime Dirty/Regenerate 链路，GeologyBlend 设为 0 时跳过新增计算并恢复原始生成公式；
- 未迁移原型的浅水、泥沙、蒸发和气象耦合 Pass：原始 `TerrainData_CS.hlsl` 在单次 Dispatch 中用 Group Barrier 后跨 Workgroup 读取输出，存在数据竞争，后续必须按固定步长拆成 Flux、Water Update、Velocity、Sediment 和 Erosion 等独立 Ping-Pong Pass；
- 验证：GTX 1050/OpenGL 4.6 下 GenerateFBM、ThermalErosion、DeriveTerrainMaps 与 Terrain 图形 Shader 编译成功；两次 GPU 输出确定性 Hash 均为 `16881604791310884879`，共 30 次 Dispatch；VS2026 `Debug | x64` 全解决方案构建成功，64 项无窗口断言全部 PASS；
- 提交：`4038de0`。

### 2026-08-11：P9 方向光阴影与 CSM

- Model 与 Terrain 共用 1～4 级方向光 CSM，包含 Practical Split、稳定正交范围、Texel Snap、可调重叠混合、`3×3 PCF`、Slope Bias 与纯运行时级联可视化；Shadow 深度资源和矩阵不序列化，只由 Directional Light 设置重建；
- Model/Terrain Bounds 在每级 Shadow Frustum 中保守剔除；Model Shadow Queue 按 Mesh 与最终 Mask 状态排序，并以独立 Shadow VAO/Instance Buffer 合批；Mask 使用最终 MaterialInstance 的纹理 Alpha 与 Cutoff 裁剪；
- 明确半透明投影策略：Opaque 与 Mask 参与方向光阴影，Blend 默认不写 Shadow Map，避免半透明表面投出错误的实心轮廓；未来若需要彩色透射或抖动阴影，作为独立能力建设；
- 建立非阻塞 `GPUTimer`、9 组 Shadow Benchmark、无人值守入口和包含 Opaque/Mask/Blend 对照、四级深度标记及远近两种构图的 Shadow Visual Validation；
- 验证：RTX 4060 完成固定 2500 实体、9 组各 30 样本性能基准；GTX 1050/OpenGL 4.6 实际窗口检查级联覆盖/过渡、Mask 镂空、Opaque 接触阴影及 Blend 无实心阴影，默认 Bias 下未见明显大面积 Acne 或 Peter Panning；VS2026 `Debug | x64` 全解决方案构建成功，立即重复同配置构建只执行增量项目检查，64 项无窗口断言全部 PASS；
- 提交：`33e0b63`、`74accc4`、`85e77a8`、`bc9af86`、`fef07a9`、`05c02df`、`a60c6d2`、`05fa519`、`02466ca`、`e8e1575`、`1d360ea`、`9ab1b44`。

### 2026-08-09：Renderer2D 空批次残留修复

- 修复实体添加 `SpriteRendererComponent` 后再移除时，视口仍残留白色 Quad 的问题；根因是空 Batch 把 `indexCount = 0` 传入 `DrawIndexed`，而底层将 0 解释为绘制完整预生成索引缓冲，导致上一帧 VBO 残留被再次提交；
- `Renderer2D::Flush` 现对零索引批次直接返回，不绑定纹理、不发起 Draw Call，也不增加统计；保留其他调用方使用 `DrawIndexed(0)` 绘制完整索引缓冲的既有语义；
- 验证：VS2026 `Debug | x64` 完整编辑器目标构建成功；55 项无窗口断言及最终汇总全部 PASS；默认 Alpine 场景在 Intel Iris Xe/OpenGL 4.6 下稳定运行，Texture、ShadowDepth、Terrain 与三个 Terrain Compute Shader 均成功加载；
- 提交：`d19b76f`。

### 2026-08-09：P8 TerrainMaterial 与分层 PBR

- 新增独立 `TerrainMaterial` 资产类型、`.glterrainmat` YAML、注册表类型与延迟缓存；格式固定包含 Grass、Soil、Rock、Snow 四层，各层保存 Albedo/Normal/AO Handle、颜色、Tiling、Metallic、Roughness、NormalScale 与 AOStrength，普通 `.glmat` 布局保持不变；
- `TerrainSpecification` 新增 TerrainMaterialHandle 并完成 Scene YAML 往返；Content Browser 可创建该资产，Terrain Component 支持拖入/清除，Viewport 可把资产应用到选中 Terrain 或创建新 Terrain，Asset Inspector 支持四层贴图、PBR 和混合参数的编辑、保存及磁盘重载；
- Terrain Shader 使用世界空间 Triplanar Mapping 混合三个投影，按派生权重叠加高度、坡度、曲率与 Flow/低地推导的湿度修正；陡坡增强 Rock、高处平地增强 Snow、湿润低地增强 Grass/Soil，并使用与 Model 一致的 Cook–Torrance 直接光、线性 HDR 输出；
- Renderer 固定使用 4～15 号纹理单元绑定四层 Albedo/Normal/AO，并在加载时严格检查 sRGB Color、Linear Normal 和 Linear Data 语义；缺失贴图退回层颜色与几何法线；
- 默认 TerrainMaterial 已接入 Grass001、Ground054、Rock027、Snow005 四套 ambientCG 1K PBR 资源；使用 OpenGL Normal，Color/AO 元数据符合采样契约，Snow 缺少 AO 时按 AO=1 回退，未支持的 Roughness/Displacement/NormalDX 保留但不注册；
- 资源配置验证：11 个运行时纹理文件、注册表路径和 TerrainMaterial Handle 引用逐项一致；直接启动既有编辑器二进制，无需重新编译，四层纹理加载无报错且 Terrain GPU 验证继续 PASS；
- 默认编辑器 Scene 恢复一个 Alpine 程序化 Terrain，但保持 `TerrainMaterialHandle = 0`；启动时会生成高度与派生图并使用 Terrain Shader 的内建四层颜色/PBR 参数，不解析 `DefaultTerrain.glterrainmat` 或加载 11 张具体材质纹理；用户主动分配 TerrainMaterial 后才进入完整纹理路径；
- VS 启动时出现的撕裂/显示异常最终确认来自显卡切换与独显选择，不应归因于 Terrain Shader；完整四层 Triplanar 的采样成本仍作为独立性能问题登记到 P8.1；
- 验证：VS2026/MSBuild 18.8.2 `Debug | x64` 全解决方案和独立测试目标构建成功；53 项无窗口断言全部 PASS；Intel Iris Xe/OpenGL 4.6 下 Terrain 图形 Shader 与三个 Compute Shader 编译成功，GPU 确定性哈希仍为 `4345498711584764525`、30 Dispatch；
- README：新增“TerrainMaterial 四层 Triplanar PBR”；ARCHITECTURE：同步资产类型、场景引用、Renderer 数据流和编辑器入口；
- 提交：`3fb5cbb`。

### 2026-08-09：P7 山脉生成、派生图与 Authoring Erosion

- `TerrainSpecification` 新增 Custom、Alpine、Plateau、Rolling Hills、Volcanic、Eroded Valley 六种状态，预设统一设置确定性 Seed、地貌参数、HeightScale 和有限次侵蚀参数；方向、宽度和台地强度支持继续手调，任一手调自动转为 Custom；
- `GenerateFBM.comp` 增加旋转后的各向异性 Ridged FBM 与双山链组合，使 Alpine/Eroded Valley 形成连续山脉走向；Plateau、Rolling Hills 和 Volcanic 使用各自稳定地貌分支；
- 新增 `ThermalErosion.comp`，每轮只读 HeightGrid ReadTexture、只写 WriteTexture，Barrier 后 Swap；最多 128 次，仅在 Dirty、Shader 热重载或显式 Regenerate 时执行，不进入每帧生态模拟；
- 新增 `DeriveTerrainMaps.comp`，一次 Dispatch 从最终 Height 生成 RGBA16F Normal/Slope、Curvature/Flow Potential 和 Grass/Soil/Rock/Snow Material Weights；权重归一化，Terrain Shader 已使用派生法线和权重进行基础可视化；所有派生纹理仅属于 `TerrainRuntime`；
- Scene YAML 保存 Preset、三项新增地貌参数、Authoring Erosion 和两个 Compute Shader Handle；旧场景缺 Preset 时按 Custom 读取。Inspector 提供预设、Mountain、Authoring Erosion 控件、生成版本和 Dispatch 数，全部复用 P6 的单命令事务；
- 验证：`scripts\Verify-Windows.bat` 完整通过，VS2026/MSBuild 18.8.2 `Debug | x64` 全解决方案构建成功且 46 项无窗口断言全部 PASS；Intel Iris Xe/OpenGL 4.6 下三个 Compute Shader 编译成功，默认 Alpine 每次执行 30 Dispatch；相同参数两次 GPU 输出哈希均为 `4345498711584764525`，高度与派生图无 NaN/Inf、范围合法且材质权重归一化；
- README：新增“山脉生成、派生图与 Authoring Erosion”；ARCHITECTURE：同步 Terrain 三段式 Compute 数据流和 Runtime 所有权；
- 提交：`4038de0`。

### 2026-08-09：P6 Terrain 生命周期与编辑事务收口

- `TerrainComponent` 的复制构造和复制赋值均只保留 `TerrainSpecification` 并清空 `TerrainRuntime`，关闭 Scene Copy、实体复制、快照恢复和属性命令通过赋值共享 GPU Runtime 的漏洞；
- Terrain、Directional/Point/Sky Light 与 Camera Inspector 已统一接入 `EditorValueTransaction`：拖动期间实时预览，控件释放时只记录一条 `ValueEditorCommand`；Terrain Undo/Redo 会恢复完整规格并强制延迟重建 Runtime；高度图与 SkyLight Cubemap 的离散替换同样进入命令历史；
- 删除从未接入当前 EditorLayer 的旧 `TerrainPanel`，正式地形参数只由 `TerrainComponent` Inspector 管理；EditorLayer 继续仅负责 Scene、Pass、Framebuffer 和面板编排，不持有 TerrainMesh、HeightMap 或 Terrain Shader 业务状态；
- 无窗口回归扩展到 35 项，覆盖 Terrain Transform/实体复制、Specification YAML 往返、Runtime 非持久化、Edit → Play Scene Copy 隔离、复制赋值失效以及单条 Terrain 命令 Undo/Redo；测试目标复用编辑器 CommandHistory 实现但仍不创建窗口或图形上下文；
- 验证：`scripts\Verify-Windows.bat` 完整通过，Premake VS2026 生成、MSBuild 18.8.2 `Debug | x64` 全解决方案构建和 35 项断言全部成功；完整编辑器在正确项目工作目录下稳定运行 8 秒；已知 GLFW Premake 弃用警告仍保留；
- README：新增“Terrain 生命周期与 Inspector 编辑事务收口”，并修正旧 TerrainPanel 说明；ARCHITECTURE：同步 Runtime 所有权、Inspector 事务覆盖和测试边界；
- 提交：`af9fea6`。

### 2026-08-09：P5 自动化回归测试与可重复构建验证

- 新增独立 `GlimmerRegressionTests` ConsoleApp，不创建窗口、Application 或渲染上下文；覆盖旧 `.glmat` 默认兼容、完整 Material 保存/重载、MaterialOverrides 启用字段合并与数值 Clamp；
- 最小内存 Scene 使用固定 UUID、Transform、ModelHandle 和完整 PBR MaterialOverrides 保存为临时 `.glimmer`，随后加载到新 Scene，并通过 `FindEntityByUUID` 验证稳定身份、组件和 Handle/Mask/Values 往返；测试临时目录位于系统 Temp，退出时递归清理，不写入默认编辑场景；
- 测试程序逐项输出 PASS/FAIL，任一断言失败返回 1；`--force-failure` 已验证调用链能稳定传播非零退出码；
- 新增 `scripts/Verify-Windows.bat` 无暂停入口及其 PowerShell 实现：绕过本机脚本执行策略后检查递归子模块是否初始化，调用仓库内 Premake 生成 VS2026 `.slnx`，自动查找或接收显式 MSBuild 路径，构建全解决方案 `Debug | x64`，最后运行无窗口测试；
- 验证：统一脚本完整通过；Premake 成功生成 `GlimmerRegressionTests.vcxproj`，VS2026/MSBuild 18.8.2 全解决方案构建成功，23 项正常断言全部 PASS；强制失败运行返回退出码 1；测试临时目录自动清理；
- README：新增“无窗口回归测试与 Windows 一键验证”，记录跨设备 Clone、子模块、生成、构建和测试流程；ARCHITECTURE：补充独立测试目标及其依赖/隔离边界；
- 提交：`c500bdd`。

### 2026-08-09：PBR 材质通道与颜色空间契约

- `MaterialProperties`、`.glmat` 与实体 `MaterialOverrides` 已同步加入 Normal/AO/Emissive Texture、NormalScale、AOStrength、EmissiveColor/Strength；旧文件缺字段时维持无贴图、Normal/AO 强度 1、Emissive 强度 0 的兼容默认值；
- 共享 Material Inspector 与实体 Override Inspector 均支持新字段、贴图拖放、连续编辑事务和 Undo/Redo；场景 YAML 保存完整 Mask/Values，MaterialInstance 缓存比较最终完整状态；
- Renderer3D 使用固定 0～3 纹理单元绑定 BaseColor、Normal、AO、Emissive，并把四纹理 GPU ID、存在状态和全部参数纳入排序/合批键；不透明 Instancing、Mask/Blend 和 EntityID 路径继续复用同一 PBR Shader；
- PBRModel 使用切线空间 Normal Mapping，AO 仅调制环境项，Emissive 在线性 HDR 空间累加；BaseColor/Emissive 使用 sRGB 资产，Normal/AO 使用 Linear，Renderer 只读取符合语义契约的纹理且不在绘制阶段改写注册表；模型切线生成对退化 UV 增加稳定正交基回退；
- Debug Rendering 新增独立 `PBRMaterialLabTool`，生成 6 个临时材质球对照 Normal、AO、Emissive、Dielectric/Metallic 与 Smooth/Rough；支持 `GLIMMER_PBR_LAB_AUTORUN=1` 自动生成并执行材质/场景 YAML 往返验证；
- 验证：Premake VS2026 重新生成成功；VS2026 `Debug | x64` 全解决方案构建成功；自动 Lab 稳定运行 8 秒并记录 `6/6 items` 渲染 PASS、旧 `.glmat` 与全部新 Override YAML 往返 PASS；测试进程正常关闭，临时文件/日志已清理；`git diff --check` 通过；
- 未覆盖：Metallic/Roughness 仍为标量，独立贴图或打包 ORM 通道、镜像 UV 的 Tangent Handedness、IBL 与阴影留给后续；
- README：新增“PBR 材质纹理通道扩展与 Material Lab”；
- 提交：`8a69243`。

### 2026-08-09：可扩展 Debug 面板与 GPU Instancing Lab

- 当前完整编辑器新增独立 `Window → Debug` 面板，以 Overview/Rendering 页签承载长期诊断入口；首个独立工具 `InstancingLabTool` 不侵入 Renderer3D；
- Instancing Lab 使用临时内存 Scene 创建真实 ECS 模型实体，退出、切换场景、进入 Play 或编辑器关闭时恢复原 EditorScene；Lab 不进入 Undo/Redo，不允许保存，默认暂停 Hierarchy 全量枚举以免大量 ImGui 行干扰渲染压力测试；
- 默认 Cube/DefaultPBR 可生成 `50×1×50=2500` 个实体；支持 Maximum Instancing、双 Roughness Material Split 和 Blend Transparent Comparison，并可拖放替换 Model/Material；
- 面板根据实体数、Submesh 数、1024 实例分块和预设计算理论 Items、DrawCall、Instanced/Individual Draw 与 InstanceCount，逐帧对照 Renderer3D Statistics 显示 Pending/PASS/FAIL，并提供首/中/末代表实体拾取入口；
- 验证：重新生成 VS2026 工程后 `Debug | x64` 全解决方案构建成功；完整编辑器稳定运行 8 秒；`git diff --check` 通过；
- README：新增“可扩展 Debug 面板与 GPU Instancing Lab”；
- 提交：`8a69243`。

### 2026-08-09：Transparent RenderQueue 与材质 AlphaMode

- MaterialProperties 增加 `Opaque / Mask / Blend` 与 AlphaCutoff，并同步 `.glmat`、实体 MaterialOverrides、Inspector、Undo/Redo 完整状态和场景 YAML；旧文件缺字段时默认 Opaque/0.5；
- Renderer3D 拆分 Opaque/Mask 与 Transparent Queue：前者保留状态排序和 Instancing，后者按实体位置到相机的平方距离由远到近稳定排序并使用普通 Draw；
- 完整编辑器固定 Opaque/Mask、Terrain、Skybox、Sprite、Transparent 的执行边界；Scene 在该宿主中延迟整个 Sprite 遍历与提交，由 EditorLayer 在 Skybox 后调用 `FlushSpritePass`；不编排 Skybox 的旧宿主仍默认立即渲染；
- RendererAPI/OpenGL 增加 Blend、BlendFunc、DepthWrite 控制；Opaque 默认禁用混合，Transparent 使用标准 Alpha 混合和只读深度，Skybox 与 Transparent 结束后恢复默认状态；
- PBRModel 按 BaseColor × Texture Alpha 执行 Mask Cutoff；Blend 的 Alpha 小于等于 `1/255` 时丢弃，避免全透明像素写颜色、深度或 EntityID；
- Stats 增加 Opaque/Mask/Transparent 项数与 Transparent DrawCall；DefaultPBR 显式记录 Opaque/0.5；
- 验证：使用 `assets/textures/balatro.png`（实际 Alpha 0～255）和真实 OpenGL 临时场景验证旧材质兼容、材质保存/重载、场景 Override YAML 往返及 Shader 编译；2 Opaque + 1 Mask + 2 Blend 得到 `5 Items / 4 Draws`，其中 1 次 Opaque Instanced Draw、2 次 Transparent Draw；
- 顺序修复：RenderDoc 抓帧确认旧实现的 Renderer2D Draw 早于 Skybox，导致透明区域先与 Clear Color 混合；现已把实际 Renderer2D Draw 延迟到 Skybox Draw 之后、3D Transparent 之前；
- 验证：VS2026 `Debug | x64` 全解决方案构建成功；顺序修复后完整编辑器稳定运行 8 秒；`git diff --check` 通过；测试场景、日志和后台进程无残留；
- README：新增“Transparent RenderQueue 与材质 AlphaMode”；
- 提交：`069fede`、`5bac5c3`。

### 2026-08-07：3D Instancing 与 MaterialInstance 缓存

- 扩展 BufferLayout、VertexArray、RendererAPI 和 OpenGL 后端，支持 PerInstance 输入、矩阵属性拆分、`glVertexAttribDivisor` 与 `DrawIndexedInstanced`；
- PBRModel 使用实例 Transform/EntityID，Shader 在编译和热重载后自检实例化契约，不兼容 Shader 自动逐项回退；
- Renderer3D 仅合并 Mesh、Shader、纹理和最终 MaterialProperties 完全一致的项，按 1024 实例分块上传动态 Instance Buffer；
- Material 与 MaterialOverrides 增加版本/Dirty，Renderer3D 以实体和材质为键缓存最终属性，并用完整状态比较保证 Undo/Redo 或遗漏标记时仍不会复用过期结果；
- Stats 增加 BatchCount、InstanceCount、Instanced/Individual Draw、SavedDrawCalls 和 Material Cache Hit/Miss；
- 验证：真实 OpenGL 临时场景中 3 个相同 Cube/Material 从 3 Draw 降为 1 Draw；单个 Roughness Override 后为 3 Items/2 Draw；再加入 2 个不兼容 Phong Shader 实体后为 5 Items/4 Draw，确认逐项回退；实例 EntityID 随 Instance Buffer 写入整数附件的 Shader 路径完成编译与运行；
- 验证：VS2026 `Debug | x64` 全解决方案构建成功；相同命令二次增量构建约 3 秒且未重新编译源码；最终无测试注入编辑器稳定运行 8 秒；`git diff --check` 通过；未删除 `bin`/`bin-int`；
- 构建修复：重新运行 VS2026 Premake，使既有 SPIRV-Cross samples/tests 排除规则同步到工程并恢复全解决方案构建；
- README：新增“3D Instancing 与 MaterialInstance 缓存”；
- 提交：`9053c6a`。

### 2026-08-05：3D Opaque RenderQueue 与状态排序

- 将 Renderer3D 从逐模型立即绘制拆为 `BeginScene`、`SubmitModel`、`EndScene`，每个 Mesh 形成包含完整材质状态、Transform 和 EntityID 的 RenderItem；
- 使用 ShaderHandle、MaterialHandle、Texture GPU ID、Mesh 生命周期地址和 EntityID 构造帧内稳定 RenderKey；
- 排序执行时缓存 Shader/Texture 状态，并在 Stats 面板展示提交、跳过、DrawCall、绑定次数及相对旧模式节省量；
- OpenGL `DrawIndexed` 不再隐式解绑 Texture2D，使纹理状态所有权回归上层渲染器；
- 验证：临时真实 OpenGL 宿主以两种顺序提交 3 个相同模型，均得到 3 个 RenderItem/DrawCall，Shader 绑定由 3 降至 1、Texture 绑定由 3 降至 1，并安全跳过 1 个无效资源；VS2026 `Debug | x64` 全解决方案构建成功；完整编辑器稳定运行 8 秒；`git diff --check`；
- 资产发现：当前仓库 AssetRegistry 的 Model 路径均缺少实际 `.obj` 文件；已修正 `.gitignore` 允许跟踪 `assets/**/*.obj`，源模型仍需从原设备或备份恢复；
- README：新增“3D Opaque RenderQueue 与状态排序”；
- 提交：`a41df51`。

### 2026-08-05：材质编辑事务与 Undo/Redo

- 建立失败感知的 `ValueEditorCommand` 与可复用 `EditorValueTransaction`，失败的 Execute/Undo/Redo 不移动历史栈；
- 实体 MaterialHandle、全部 Override 开关/数值、纹理拖放/清除和 Reset 使用完整 `MaterialComponent` 快照；
- 共享 `.glmat` 使用完整 `MaterialState` 事务，连续控件编辑压缩为单条命令，Undo/Redo 同步内存和磁盘；
- Material 保存改为临时文件、备份和替换流程，失败时恢复内存与原文件；Play 模式下共享 Asset 只读；
- 验证：VS2026 `Debug | x64` 全解决方案构建成功；原生冒烟测试通过保存/重载/Undo/Redo及文件锁定失败回滚；编辑器在 Intel Iris Xe/OpenGL 4.6 下完成初始化并稳定运行 8 秒；`git diff --check`；
- 已知警告：保留既有 C4244、C4267 和 `strncpy` C4996 警告；
- README：新增“材质编辑事务与 Undo/Redo”；
- 提交：`d705f58`。

### 2026-08-05：建立三文档同步制度

- 将 `PROJECT_STATUS.md`、`ARCHITECTURE.md` 和 `README.md` 确立为进度、架构事实和功能说明三个互补的信息源；
- 要求每次任务完成前同步审查三份文档，并按职责更新，避免不同设备和会话获得过期上下文；
- 在根 `AGENTS.md` 固化读取、更新和完成检查规则；
- 验证：三份文档职责与更新触发条件已交叉核对，`git diff --check`；
- 提交：`85798df`。

### 2026-08-05：架构文档同步

- 以当前源码重写 `ARCHITECTURE.md`，补齐构建、应用生命周期、资产、场景、编辑器与现代渲染链路；
- 明确当前完整编辑器、较早宿主、OpenGL 可运行后端与 Vulkan 接口预埋之间的边界；
- 同步记录 MaterialInstance、Edit/Play、CommandHistory 和尚未实现的 RenderQueue/材质事务；
- 验证：文档结构与关键源码逐项核对，`git diff --check`；
- 提交：`85798df`、`0022057`。

### 2026-08-05：项目品牌与 Windows 应用图标

- 将临时 Logo 素材迁移到 `resources/branding/`；
- 生成透明标准 PNG 和包含 16–256 px 的 Windows ICO；
- 使用共享 `GLFW_ICON` RC 资源接入 Sandbox 和两个编辑器；
- 从生成的 EXE 成功提取图标验证；
- 清理原 `tmp/logo/`；
- VS2026 `Debug | x64` 全量构建通过；
- 提交：`3b14bd8`。

### 2026-08-05：MaterialInstance 与实体材质 Override

- 建立基础材质与实体局部覆盖的合并模型；
- 支持 BaseColor、BaseColorTexture、TilingFactor、Metallic、Roughness；
- 接入 2D、3D Renderer、场景复制和 YAML 序列化；
- 分离实体 Override Inspector 与共享 Material Asset Inspector；
- 提交：`51543e7`。

### 2026-08-04：编辑器基础收口与 Undo/Redo 基础

- 建立 SelectionContext 与实体/资产选择边界；
- 建立 CommandHistory、实体快照、创建/删除/复制撤销；
- Transform 连续编辑可以生成单个命令；
- 完善组件添加、移除与 Edit/Play 状态边界；
- 相关提交：`ed32a17`、`d4ffeef`、`167f4f2`、`094d7a0`。

### 2026-07-31：跨设备 Shader BOM 兼容

- 文件型 Shader 与 Compute Shader 加载时剥离 UTF-8 BOM；
- 修复部分 OpenGL 驱动拒绝 `EF BB BF` 的问题；
- 初始编译和热重载共用兼容路径；
- 提交：`19f343d`。

### 2026-07：资产、场景与编辑器工作流

- UUID 稳定实体标识和 Edit/Play 场景恢复；
- AssetHandle、资产注册表、内容浏览器和拖放导入；
- Material、Shader、Texture、Model、Cubemap 资产接入；
- 场景序列化、原生文件对话框、Gizmo、EditorCamera、鼠标拾取；
- 编辑/播放模式和场景实体化地形工作流。

### 2026-07：现代渲染基础设施

- Framebuffer 多附件、实体 ID 附件和 Pixel Readback；
- Uniform 缓存与 UBO；
- Compute Shader、GPU Readback 和 Shader 热重载；
- 多 Pass 渲染、程序化地形和 Terrain Renderer；
- 统一光源组件、Light UBO、基础 PBR；
- 线性 HDR、ACES Tone Mapping、Skybox 与 SkyLight；
- Vulkan/SPIR-V 接口预埋。

### 基础引擎能力

- Premake 构建系统与 VS2026/v145 工程；
- Application、Window、Layer、Event、Input 和日志系统；
- OpenGL RendererAPI 抽象；
- 2D 批处理、纹理槽、相机与基础 Shader 系统；
- ECS、组件、场景和 ImGui 编辑器框架；
- GLFW、Glad、ImGui、GLM、EnTT、yaml-cpp、ImGuizmo、SPIRV-Cross 等依赖集成。

## 已知问题与技术债

### 构建与依赖

- VS2026 Premake 生成的主入口是 `GlimmerEngine.slnx`；旧 `GlimmerEngine.sln` 可能来自 VS2022 或早期生成，自动验证脚本优先构建 `.slnx`；
- GLFW、Glad、ImGui、yaml-cpp、ImGuizmo 与 SPIRV-Cross 的 Premake 项目由主仓库 `scripts/premake/Dependencies.lua` 统一定义；子模块只提供源码，不应再写入本地 Premake 或 Visual Studio 工程；修改适配层时必须复验 SPIRV-Cross CLI/samples/tests 排除规则；
- Assimp 是独立生成且不提交产物的静态依赖；Glimmer 的 PreBuildEvent 与 `Verify-Windows.ps1` 会检查产物、构建 Schema、配置、子模块提交和 ccache 状态，过期时自动调用 Ensure/Build 脚本。仍可手动运行 `scripts/Win-BuildAssimp-vs2026.bat Debug|Release` 强制重新配置依赖。
- `assets/models/Cerberus` 已提交约 175 MiB 的 FBX/TGA 测试资源，但仓库缺少原许可说明；公开分发或商业使用前必须补齐明确的再分发许可，否则应从发布资产与版本化回归中替换为自有小型样本。
- Model 资源已恢复 Cube、Plane、UV Sphere、bunny、planet、spacecraft、suzanne；注册表中的 `models/New Folder/Cube.obj`、`models/dragon.obj`、`models/UV Sphere.obj` 仍缺少源文件，需要从原设备恢复或移除失效条目。
- FBX 当前仅支持静态网格并把节点变换烘焙到顶点；尚无单位归一化、保留层级、骨骼/动画、Morph Target、嵌入纹理、自动 `.glmat`/`.glmesh` 烘焙或 glTF/GLB 注册。

### 编辑器

- Undo/Redo 已覆盖实体生命周期、组件增删重置、Transform、Material、Terrain、Light 与 Camera；Tag、SpriteRenderer、ModelRenderer 等部分属性仍有直接修改路径；
- 编辑器已具备 Scene 内容级 Dirty、未保存确认和事务式文件替换；尚未检测场景文件被外部程序修改后的保存冲突，Scene 根名称仍固定为 `Untitled`；
- Material Asset 已具备保存、撤销和失败反馈；TerrainMaterial 可显式保存/重载但尚未接入 Asset Command/Undo 和统一退出 Dirty 提示；其它共享 Asset 仍缺少统一保存协议；
- 连续组件编辑统一采用激活快照/释放提交边界；Inspector 控件和 Viewport Gizmo 均不会逐帧创建命令，后续连续控件也应复用该约定。

### 渲染

- P14 全尺寸视觉基线在 120 步读到最大 Velocity 44320；`HydrologyUpdate` 用前后水深平均值（下限 `1e-6`）作分母，近干格与当前 Source 的关系需要专项核查。水面泡沫已对只读速度和水深做有限值、干格与范围保护；模拟端的速度定义仍待核查；
- Renderer3D 已有 Opaque/Mask/Transparent Queue、状态排序、Opaque Instancing 和 MaterialInstance 缓存；Transparent 首版仍按实体原点而不是 Mesh Bounds 中心排序，且不支持透明实例化或 OIT；
- 模型材质已支持多 Pass、通用 Float/Float4 参数和 None/Back/Front Cull；Pass 目前通过 `.glmat` YAML 编辑，Inspector 尚无 Pass/参数列表 UI，实体 Override 也不覆盖共享 Pass 参数；透明多 Pass 仍按逐项透明队列执行，法线外扩宽度使用世界单位而不是屏幕像素；
- AlphaMode Shader 契约当前由 PBRModel 完整实现；自定义 3D Shader 若要正确支持 Mask/Blend，仍需自行声明并使用 `u_AlphaMode`、`u_AlphaCutoff`；
- PBRModel 已支持 BaseColor、Normal、AO、Emissive 以及 FBX 导入的独立 Metallic/Roughness Texture；`.glmat` 尚未暴露 Metallic/Roughness 纹理 Handle，也未定义 ORM 打包通道；当前 Vertex Tangent 不包含镜像 UV 所需的 Handedness；
- Renderer2D 仍固定使用 TextureShader，`.glmat` 的 ShaderHandle 尚未参与批次兼容判断；
- 完整编辑器的 Sprite 统一在 Skybox 后、3D Transparent 前 Flush；Renderer2D 尚无独立 AlphaMode、透明距离排序或与 3D Transparent 的跨队列排序，零 Alpha 的 EntityID/深度语义仍需后续单独收口；
- Terrain 已完成按世界尺寸自适应的 Chunk、三档距离 LOD/迟滞/相邻约束/Skirt、Color/Shadow 剔除、四层 Triplanar PBR、固定步水文与 Runtime Erosion；运行时 Height 会刷新 Normal/Slope、Analysis 和 Material Weights。尚无显式 Bake，模拟结果关闭或重建后丢弃；
- P16 源码审查：Authoring Talus 使用归一化高度差而非世界坡角，曲率 Laplacian 未按世界间距归一化；派生正曲率代表局部凹地，但材质负曲率分支增加湿度/土壤，符号契约待统一并以碗形/凸丘验证。Analysis Flow 仅为局部最大落差，不是上游汇流累积；Normal/Analysis/Weight 在顶点读取后插值，近景精度受网格约束。近远分层高度、跨块模拟边界与连续几何过渡尚未实现；
- CSM 已完成 Practical Split、Texel Snap、可调重叠混合、基于 Bounds 的 Shadow Frustum 剔除、运行时级联着色、Alpha Mask 投影和每级 Model Instancing；Terrain 仍独立提交，Blend 默认不参与 Shadow Pass，尚无彩色透射或抖动式半透明阴影；
- SkyLight 已支持六面 LDR/等距柱状 HDR、线性 `RGBA16F`、完整普通 Mip Chain、内存派生缓存，以及 Model/Terrain 共用的 Diffuse Irradiance、GGX Specular Prefilter 和 Split-Sum BRDF LUT；尚无持久化磁盘缓存、环境旋转、局部 Reflection Probe 或动态场景反射；
- P14 已完成 CPU/GPU 场、TerrainRuntime 所有权、固定步调度、四场诊断、GPU 趋势 Contract，以及 Rainfall/Evaporation 到 P13 Water 的守恒耦合；水面/瞬时岸线视觉基础已接入；气候材质权重反馈和植被实例化仍未实现；
- Water Surface 当前使用不透明/天空快照和既有 Terrain LOD，尚无水下透明折射、分层水体透射、SSR、水面投影阴影、独立水体 LOD/逐块水量剔除、持续湿润历史或水流 Motion Vector；浅水边缘受网格细分限制，空水域仍会提交表面 Draw 后在片元阶段剔除；
- Vulkan 目前只有接口和依赖预埋，没有可运行后端。

## 固定验证清单

根据任务范围选择必要项；触及核心构建、场景、资产或渲染时应执行完整清单。

- [ ] 运行 `scripts\Verify-Windows.bat`（统一执行子模块检查、Premake VS2026 生成、`Debug | x64` 全解决方案构建和无窗口回归）
- [ ] `git diff --check`
- [ ] 相关应用至少启动并保持运行到首帧
- [ ] 场景保存 → 关闭/重载 → 状态一致
- [ ] Edit → Play → Stop 后编辑场景恢复
- [ ] Undo → Redo 往返结果一致
- [ ] 记录未解决警告、失败或未执行的验证

## 任务完成记录模板

完成新任务时，将模板内容合并到“已完成里程碑”，并从当前/后续区域删除原任务。

```markdown
### YYYY-MM-DD：任务名称

- 完成：<核心能力或行为变化>；
- 关键文件：`path/to/file`；
- 验证：<构建、运行、序列化或性能结果>；
- 未覆盖：<明确留给后续的边界，没有则删除此行>；
- README：<新增或更新的章节名称>；
- 提交：`<commit>` 或“待提交”。
```
