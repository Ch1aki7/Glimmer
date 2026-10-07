# Glimmer 项目状态与长期工作台

> 这是本项目跨设备、跨会话的执行入口，不是完整技术文档。
> 开始工作前先阅读本文；需要实现细节时再查阅 `README.md` 中对应的功能章节与 `ARCHITECTURE.md`。

## 文档状态

- 最近更新：2026-10-07
- 当前分支：`main`
- 当前构建环境：Visual Studio 2026、v145、Windows x64
- 当前默认验证配置：`Debug | x64`
- 当前主线：P17 通用分层地形创作与组合
- 主线状态：P17-A/B 已验收；P17-C1/C2 已验收；当前实施项 P17-C3 验收与成本复核，待开始；P16-A 为已验收基线

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

### P17：通用分层地形创作与组合

**状态**：进行中；P17-A1/A2/A3 与 B1/B2/B3 已移入里程碑，A/B 已验收；C1/C2 已移入里程碑，当前实施项 P17-C3（验收与成本复核），待开始。2026-10-03 用户明确调整方向，本方案替代旧 P16-B～F 的线性实施顺序。P16 已验收部分继续作为基线，未实施部分不记为完成。

**目标**

- 在现有高度场渲染基础上，提供可复现的基础地貌、局部地形印章、保护遮罩与可选细节组合，让编辑器和游戏宿主均能消费同一引擎能力；
- 先以单个有限 Terrain、全局高度输出建立闭环，再增加局部缓存与连续几何；不以完整河网、侵蚀、气候、无限世界作为首版前置；
- 保留旧场景与 Data/Synthesis 版本语义，以明确的新配方路径渐进迁移，不先删除现有 Terrain。

#### 当前源码基线与迁移清单

本方案基于清理后的工作区。P16-A 的端点采样、尺度派生与 CPU 分层/预算工具仍在；B1 等价 Tile 未保留，不继承上轮工作区的 B1 验收结论。

| 已有实现 | 新方案中的处理 |
| --- | --- |
| 条件分形 v2、五预设、导入高度图 | 保留为基础高度来源；首版印章只接入 Data v2 程序化路径，导入路径维持原行为，后续显式适配 |
| Data v2、TerrainSampling、派生图 | 保留端点与局部单位契约；CPU Compose 工具不等于已实现运行时分层合成 |
| TerrainComponent/Runtime、Scene YAML、复制隔离、Undo | 复用持久规格/非持久资源边界，增加配方数据与整体事务 |
| Chunk、三级地形 LOD、Skirt、Color/Shadow/Water | 保留地形绘制基线；水面 W 已独立统一细分；Chunk 不成为创作模块边界；地形连续 Morph 尚未实现 |
| TerrainGenerator 的 FBM→Thermal→Stamp→Derive | A2 已接入有序解析印章；受保护的后置侵蚀后续实现 |
| Runtime 水文/气候 | C1 已实现显式 Static/Simulation，Static 不创建模拟对象；Simulation 中暂停仍保留资源，完整清单及峰值复核待 C3 |
| 四层 TerrainMaterial | 首版复用；区域材质覆盖与生态散布后续独立实施 |

当前高度/派生/模拟仍为全局同分辨率网格，没有按需 Tile 缓存、高频局部数据或连续几何。默认 1024 范围/1024 节点间距约 1.001 局部单位；现有默认 LOD0 几何间距约 2.98，不能仅提高高度精度便宣称近景轮廓改善。

#### 引擎、编辑器与游戏职责

| 层 | 负责 | 不纳入本地形主线 |
| --- | --- | --- |
| 引擎 Terrain | 配方数据、基础高度、印章/遮罩合成、确定性与版本、派生数据、地表查询接口、缓存失效、生成诊断 | 任务类型、营地含义、阵营、敌人、出生/撤离、战斗节奏与胜负规则 |
| 引擎 Renderer/Simulation | 地形网格/LOD/剔除/阴影与水面采样；可选模拟、资源生命周期和预算 | 根据任务难度改变地貌；相机驱动重新随机生成内容 |
| 编辑器 | 通用印章列表、参数与摆放预览、Undo/Redo、保存/重载、灰模与遮罩诊断 | 内置某款游戏的任务生成器；在 EditorLayer 或 Inspector 实现合成算法 |
| 游戏宿主 | 根据任务规则选择配方，计算地点与路径需求，提交普通地形操作；消费结果后布置游戏实体并验证玩法 | 直接改写生成器纹理、缓存或模拟内部状态 |

平台、盆地、坡道、走廊是几何操作；“平台作为撤离点”“走廊必须连接两个任务目标”是游戏规则。通用碰撞、导航与 Prefab 若后续建设，应作为独立引擎子系统；本主线不假设它们已存在，也不把完整玩法可达性当作地形几何验收。编辑器验收场景仅使用中性几何案例。

#### 数据模型与模块边界（目标契约，实现进度见阶段表与里程碑）

- 首版沿用 TerrainSpecification 保存配方值，不先引入新的资产注册类型；Recipe.Version 管组合语义和有序操作列表，基础来源与 Seed 由既有 Specification.Procedural/HeightMapHandle 与 Noise.Seed 唯一保存，不复制第二份配置。DataVersion 管采样，SynthesisVersion 管原噪声，Recipe.Version 管组合语义，三者不互相替代。旧 YAML 缺配方时保持原链路；未知配方版本明确报错并禁止静默降级保存。
- TerrainStamp 描述通用操作：稳定 ID、启用、XZ 中心、绕 Y 旋转、局部尺寸、过渡宽度、强度及高度参数。首版只实现解析 Ellipse/Rectangle 轮廓与 Add/SetHeight；平台和盆地由这些操作表达，不增加 Camp/Objective 等游戏枚举。高度图印章、Spline、区域材质覆盖后续扩展。
- 操作参数采用 Terrain 局部单位，目标高度是绝对局部 Y，Add 是高度增量。评估时转为当前 HeightScale 对应的存储值；首版合成结果限制在既有可渲染高度范围内并报告裁切，不悄悄突破 Color/Shadow/Water Bounds。HeightScale 为零、非有限参数和零尺寸必须有显式拒绝语义。
- 在引擎 Terrain 层增加不依赖编辑器/Scene 的纯数据验证与 CPU 参考评估工具；GPU Pass 使用相同数据契约。TerrainGenerator 负责调度与静态输出；Renderer 消费结果，后续逐步收拢当前 Prepare 的生成/模拟协调，首版不同时重写整个渲染器。
- Runtime 只持有由配方重建的 Height/派生/临时纹理及内容版本；Scene 保存配方，不保存 GPU 缓存。实体复制和 Edit/Play 隔离继续清空 Runtime。生成失败不得发布半套新 Height/法线/权重；保留上一份完整资源并报告失败，首帧无结果时明确跳过。
- 生成内容版本由地形身份、配方及其依赖变化决定；相机只影响绘制/驻留。重载 Shader 后重新验证并失效对应结果；普通帧无变化时不重新 Dispatch、不做同步读回。

#### 合成顺序与保护契约

```text
基础高度（复用现有生成与可选基础热蚀）
  → 有序印章合成与保护遮罩
  → 可选受保护的创作修饰/细节（后续阶段）
  → 最终静态高度 Hstatic
  → Normal/Slope、Analysis、MaterialWeight
  → 可选 Runtime 模拟从 Hstatic 初始化
  → Color / Shadow / Water 与查询消费同版本表面
```

首版把原热侵蚀放在印章之前，印章之后直接派生，不以“后置侵蚀再重压平台”隐藏数据不一致。后续后置修饰必须显式读取保护遮罩。运行时侵蚀是否允许改变受保护区是独立选项；未实现保护前，印章验收暂停模拟，C 完成后使用显式静态模式，不能宣称模拟中平台仍保持形状。

- 在印章局部坐标计算轮廓和外侧平滑过渡，得到权重 w∈[0,1]。Rectangle 采用有符号距离，Ellipse 采用按短半轴缩放的径向度量，椭圆过渡不保证精确等距。Add：H'=H+w×Delta；SetHeight：H'=mix(H,Target,w)。强度进入 w；重叠按保存列表顺序评估，相同 Priority 或哈希不用于隐式排序。过渡外严格为零，核心区强度为 1 时满足目标；CPU/GPU 精度采用容差比较。
- 保护是独立于高度的通道，可由操作遮罩合并为 P=max(P,w)。后续细节使用 (1-P) 衰减；侵蚀保护需要另外定义交换限幅与守恒，不能直接丢弃流量/泥沙。P17-A 不必分配尚无消费者的常驻保护图。
- 后续分层渲染采用 Hrender=Hstatic+DetailResidual+Resample(HsimCurrent-HsimInitial)，禁止重复相加 Runtime 绝对高度。首版无 DetailResidual，仍使用单个最终 Height；改变静态配方明确重建模拟初值并 Reset，不保留不兼容的旧水量。
- 创作印章可跨任意 Chunk/Tile；每个采样位置用同一 Terrain 局部坐标求值，不能逐块归一化。Halo 只服务有限半径派生核，不代表局部侵蚀拥有足够边界条件。

#### 阶段顺序与验收

| 阶段 | 状态与交付 | 验收后才进入下一阶段 |
| --- | --- | --- |
| P17-C 生命周期与可选模拟 | **当前 C3，待开始**；A/B/C1/C2 已验收；显式静态/模拟模式、独立初始高度、失效/Reset、静态模式不分配水文/气候；新模式默认静态，旧场景保留原行为 | 静态资源清单/成本；切换模式、Reset、重载和 Edit/Play 隔离；原模拟 GPU Contract 不退化；Color/Shadow/Water 帧内版本一致 |
| P17-D 局部数据与连续几何 | 依赖 A～C；先重建 Detail=0 等价 Tile，再按需缓存/高频残差、屏幕误差 LOD/Morph | 同级/跨级接边、Halo、重载/淘汰确定性、加载回退、移动视角、近景轮廓及峰值预算；Skirt 不代替连续几何验收 |
| P17-E 资产印章 | 依赖 A～D；高度/遮罩 Texture Asset 印章、依赖版本与重建、局部 UV/过滤/高度基准契约；明确静态高度与细节层应用顺序 | 缺资产/热重载反馈、变换/跨块连续、持久配方和 Undo；地形 Height 资产必须为线性数据语义；不自动生成任务实体，不隐含完整 Prefab |
| P17-F 性能与总验收 | 依赖 A～E；确定目标设备默认质量，核对生成/派生/模拟/Color/Shadow/Water 分项、内存峰值和兼容 | 数值、视觉、持久化、失败回退与资源门槛全部通过；完成三文档同步后才结束 P17 |

A/B 的完整完成记录见里程碑；首版限制 64 个操作，超限明确反馈；每印章一个 Ping-Pong Dispatch，当前只验证正确性，确认耗时后才考虑分块索引/批量评估。

**P17-C 剩余实施清单（C1/C2 已移入里程碑）**

1. **C3 当前项：验收与成本复核**：检查静态资源清单、模拟分配次数、模式往返/Undo/重载/Edit-Play 隔离、首次失败与旧结果保留；复跑原水文/气候 GPU Contract、印章/查询契约及固定视角兼容。分别记录静态/模拟纹理成本与重建峰值，再决定是否需要模拟降采样；不将暂停模拟解释为无资源，也不把本阶段记为侵蚀保护已实现。

#### 水面专项状态

2026-10-07 用户明确将本轮优先级调整为完整水面构建。W1 连续视觉采样、W2 波纹法线/外观资产、W3 泡沫/岸线及 W4 独立统一细分已完成并移入同日里程碑；水面最终方案与证据集中在完成记录，使用说明见 README。W4 采用同一 Terrain 全块统一质量，避免依赖未完成的 D/Morph；不宣称实现屏幕误差自适应 LOD。水面专项结束后恢复 P17-C3，完整地形/模拟资源和成本复核仍待开始，不以水面统计替代 C3/F 总验收。

#### 统一验收与预算

- 正确性：空操作列表与原路径等价；NaN/Inf 输出数量为零；单位和 HeightScale 换算用解析平台/斜面验证；重叠顺序、禁用操作、地形边界与旋转轮廓有明确预期。派生权重有限、非负、归一化；已有更严 Contract 不放宽。
- 可复现：Seed/算法/配方版本/操作顺序/输入资产固定；保存→重载、Undo→Redo、Scene Copy 和 Edit→Play→Stop 恢复相同规格。同机同驱动 GPU 重建可比哈希，跨厂商不承诺浮点字节相同。
- 表面：固定相机/光照先灰模再材质；用跨 Chunk 平台与盆地查看接缝、轮廓、法线、阴影和水面。移动 LOD 验收在 D 完成，不拿静止截图代替。
- 候选数值门槛：A 的 CPU/GPU 合成高度差 ≤1e-5×max(HeightScale,1)，核心平台目标高度同门槛；旋转/过渡轮廓以解析采样位置验证。D 的同级边界采用同门槛、法线差 ≤1e-3，跨级需在完整 Morph 区间保持边位置一致。平台/通路坡度上限由调用者输入并单独统计，不写死为某款游戏的通行标准；门槛需在目标设备实测，不作为本次通过结果。
- 查询：首版 CPU 快照由显式请求获取，不在普通帧读回；高度查询的采样/三角化语义必须注明，连续高度场插值不自动等于当前 LOD Mesh 碰撞面；碰撞/导航系统接入另立验收。
- 成本：空配方生成纹理约 32 字节/格；有效印章加裁切/保护图后约 40。水文约 76、气候约 40，空配方共约 148 MiB/1024²（不含 Mesh、CPU、驱动及临时资源）。C1 Static 已免去模拟对象；完整纹理清单、实际峰值与性能节省待 C3 核验，不据估算宣称实测收益。单张 R32F 遮罩或 CPU 高度快照约 4 MiB/1024²，候选、Ping-Pong、快照副本、临时和输入资产逐项记账。
- 首版沿用 1024²；不默认扩至 4096²。候选压力案例为 0/1/16/64 个操作，测完整生成、Dispatch 与显存峰值；D 再比较 8/16/32 个 Tile 的候选档位。目标设备实测后设时延/显存阈值，不在本方案承诺帧率。
- 代码阶段验证：VS2026 Debug x64 完整构建、无窗口数据/持久化回归、真实 GPU 数值与视觉 Fixture；覆盖首次加载、Shader 编译失败、缺资产、Resize、Reset 和旧场景。图片/日志仅保留本地，不新增项目报告/截图归档。

#### 旧主线的处置与停止条件

- P16 的条件分形、预设高度与固定视角验证、P16-A 保留在已完成里程碑；P16 总验收未完成，不新增虚假完成记录。旧 B1～B3 目标并入 P17-D，旧 B4 模拟降采样作为 C/D 后按预算触发的可选优化，不先承诺实施。
- 旧 P16-C 河网/湖泊、D 多尺度创作侵蚀/历史场、E 地貌生态权重降为可选自然地貌扩展；它们不阻塞混合创作闭环。配方持久性提前到 A，性能总验收归 F。现有 P14 气候/植被主线后续任务保留，其依赖从已完成的 P17 能力中逐项确认。
- 本方案不建设整款游戏，不要求先实现完整任务布局、战斗 AI、无限世界、体积地形或联网破坏。洞穴/悬挑由独立 Mesh 或未来体积系统承担。
- Spline、区域材质覆盖与通用散布作为 A～F 以外的候选扩展，需求确定后另立交付与验收，不把“按需要”能力隐含在 P17 总验收内。
- 高度越界/接缝/派生不同步/版本重建错误时停止下一阶段，保留旧路径；成本超预算优先限制操作量、局部细节与驻留数。生成器不为游戏放宽数据契约。
- 旧实现只在替代路径完成兼容验收、用途和调用者清理后逐项移除。当前没有删除生成器、模拟器或渲染器的必要。

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

**排序与技术依赖**：现有排期仍在 P13/P14 后；图像解码本身不依赖气候算法。若跨格式模型资源成为实际阻塞，可单独提升需要的切片。P18 骨骼/动作核心不依赖完整 P15；模型纹理资产化与无导入库发布依赖 P15 的解码/纹理烘焙契约，不要求先完成 EXR/Cubemap 全链路。

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

**供模型动作建设复用的切片（均未开始）**

| 切片 | 交付与边界 | 对 P18 的影响 |
| --- | --- | --- |
| P15-A 图像解码契约 | 后端无关 ImageData/ImageDecoder，方向/像素类型/显式语义；最小 OpenImageIO 构建与现有 LDR 回归 | P18-A～H 可先用现有 PNG/JPEG/TGA/BMP 路径；图像能力不足时按实际资产触发此项 |
| P15-B 内部 Texture2D 资产 | 版本化导入设置/烘焙、AssetHandle/依赖、颜色空间/Normal/通道映射、嵌入图像到资产转换 | P18-I 模型材质收口与 J 无源文件发布的硬依赖；不另写模型专属解码器 |
| P15-C HDR/EXR 与 Cubemap 迁移 | 接入等距柱状与六面导入、环境派生兼容 | 不阻塞骨骼、VMD、重定向；仍计入 P15 完整验收 |
| P15-D 发布与旧路径清理 | 验证无 OpenImageIO/plugin DLL 的内部纹理加载，兼容完成后移除 stb_image | P18-J 联合发布验收需要无导入依赖的纹理加载；全局旧路径清理不阻塞开发预览 |

### P18：通用模型、骨骼动画与动作适配系统

**状态**：方案完成，代码未开始；A 为首个可实施切片，其余按依赖验收推进。2026-10-06 用户要求加入引擎建设，仅完善文档；当前唯一主线为 P17-C，不自动切换优先级。

**目标与排序建议**

- 建立“模型/骨架”和“动作片段”分开导入、显式绑定、统一播放的通用能力；支持 glTF/GLB、FBX 与 PMX/VMD，不把动画系统绑定到某种模型格式。静态 OBJ/FBX 及现有材质、场景和渲染继续兼容。
- 先完成 A～D 的通用骨骼动画闭环，再接 E/F 的 MMD 核心、G 的跨骨架重定向、H 的物理和 I/J 的创作/发布。不同模型的骨架不兼容时不假定“同名即可播放”。
- 技术上不依赖 P17-D/E 的地形缓存/资产印章，也不依赖 P14 植被。若用户后续将角色优先级提升，可在 P17-B3/C 和已有地形成本复核后切入 P18-A；这只是候选交付顺序，不把未完成 P17/P14/P15 记为完成或撤销。
- 不先完成整个 P15。A～H 的骨骼、动作与几何测试可用程序生成网格及现有 LDR 贴图；I/J 的依赖收口与运行时发布需要 P15-A/B 及内部纹理运行时验收。EXR/Cubemap 不成为播放动作的前置。

#### 当前基线与迁移

- ModelImporter 仅注册 OBJ/FBX，AssimpModelImporter 使用 PreTransformVertices；MeshSource/MeshVertex 只有静态位置、法线、vec3 切线与 UV，无骨骼、蒙皮权重、层级或 Morph。AssetType 无 Skeleton/AnimationClip/RetargetProfile/AnimatorGraph；Scene 无动画组件/Runtime。
- Assimp v6.0.5 构建仅启用 OBJ/FBX/GLTF，glTF/GLB 未向资产层开放，MMD 未启用。本地 MMDImporter 实际入口只有 PMX；存在 VMD parser 源码不等于已接入 VMD 动画。其 PMX 适配输出网格、骨骼及部分材质，不能代替完整 Morph/IK/SDEF/QDEF/物理适配。
- Model 直接持有导入纹理，尚未转换为 AssetHandle；这部分由 P15/P18-I 收口。已有 Model Shader ABI、多 Pass/Toon 法线外扩、Opaque/Mask/Transparent、Shadow Instancing 可复用，但当前没有形变输入和实例独立姿态。Camera Velocity 只表示相机运动，不能代替角色蒙皮/Morph 速度。
- 保留现有静态导入配置；新增显式 Static/Animated 导入配置。Animated 路径禁止 PreTransformVertices 丢失层级，完成兼容前不删除旧 ModelImporter/Model/Mesh，不把 Assimp 类型引入公共引擎 API。

#### 职责、资产与实例边界（目标设计，尚未实现）

| 层 | 职责 | 边界 |
| --- | --- | --- |
| Asset/Importer | 格式解析、CPU ModelSource/SkeletonSource/AnimationSource、坐标与单位转换、依赖/诊断/内部烘焙 | 无 GPU 上传，不播放、不修改 Scene；第三方类型私有 |
| Animation 核心 | 层级姿态、曲线采样、绑定、混合、重定向、约束/IK、事件及 RootMotionDelta | 不写渲染资源，不决定走路/攻击/死亡等玩法规则 |
| Scene | 每实例 AnimatorRuntime、一次更新的时序、根运动交付与骨骼附件 | 只保存资产 Handle/设置，运行姿态不进入 YAML；Edit/Play/复制独立 |
| Renderer | 只读帧内已发布姿态/Morph、蒙皮和形变 Bounds、Color/Shadow/拾取/法线/速度一致性 | 不在 Draw 或每级 Shadow 内推进动作；不按模型材质合并不同姿态 |
| Physics/Constraint | 通用刚体/关节、固定步、骨骼与刚体转换；MMD adapter 解释 PMX 约束 | 当前物理系统未实现，不拿地形水文模拟替代角色物理 |
| 编辑器 | 导入设置、骨骼/Morph/轨道预览、时间轴与映射、诊断、Undo/Redo | 不把格式解析和求解器写入 EditorLayer/Inspector |
| 游戏宿主 | Animator 参数/状态驱动、动作事件处理、移动/碰撞与角色行为 | 不直接改骨骼缓存/GPU palette，不把示例动作状态当引擎固定规则 |

- 拟用不可变 ModelAsset（网格/节点/Skin/Morph/材质依赖）、SkeletonAsset（拓扑/稳定节点 ID/参考姿态）、AnimationClipAsset（曲线与源绑定描述）、RetargetProfileAsset（源/目标骨架版本与映射）及后续 AnimatorGraphAsset。模型/动作从同一 FBX/glTF 拆出时用稳定子资产身份，不能仅靠导入序号；Skeleton 可共享，但 Skin 的 inverse-bind 与 mesh-to-skeleton 变换属于具体模型。
- AnimatorComponent 保存 Model/Skeleton/Clip/Graph/Profile Handle 与播放/根运动设置，旧 YAML 缺组件仍走静态路径；运行游标、当前/前帧姿态、Morph 权重和物理状态只在独立 Runtime。缺资产/未知版本/不兼容骨架拒绝发布半套依赖，保留旧完整实例并给出原因，首次失败显示参考姿态或明确不绘制。
- 骨架签名覆盖稳定节点 ID/父索引/参考局部变换与版本；动作绑定记录目标签名、映射及算法版本。重导入骨架、动作或 Profile 时失效绑定和 GPU palette；同名重复骨骼必须显式路径/ID 消歧。原始名称保留，编码转换不偷偷改名；VMD 名称截断碰撞必须报告。
- 内部模型/骨架/动作/映射与纹理均采用版本、导入器/算法版本、源与依赖哈希、设置和稳定 Handle；未知 schema 拒绝静默降级。派生缓存可重建，资产事务保存/依赖发布与 Undo 要保留旧结果；发布运行时读取内部资产，不依赖 Assimp、PMX/VMD parser 或 OpenImageIO。

#### 数学、采样与更新契约

- 导入阶段将坐标、手性、单位、UV、绕序和切线 Handedness 转为同一引擎约定，模型/骨架/动作/物理同步转换且只转换一次；默认单位未确定前必须显式保存源到引擎缩放。非均匀/负缩放、shear 与不可逆参考矩阵必须有支持或拒绝策略，禁止仅缩放顶点而漏掉骨架/动画/刚体。
- 父索引/拓扑合法且无环；参考局部 TRS、全局参考矩阵、Skin inverse-bind 与网格空间变换分开。列向量约定下，网格局部蒙皮矩阵采用 inverse(meshGlobal) × jointGlobal(t) × inverseBind；参考姿态重现作为首项验收，不能一律忽略 mesh node 或额外重复乘 Entity Transform。
- 保留原始影响集合；首版 GPU LBS 使用四影响配置并报告裁剪/归一化，超过上限默认拒绝，只有显式导入选项才允许误差受控的简化。无影响顶点明确绑定参考节点，非法 joint/负权重/NaN/零权重和诊断；后续八影响、palette 分段与 SDEF/QDEF 各自声明版本，不静默用 LBS 替代。
- 动作时间统一秒；glTF 原插值 STEP/LINEAR/CUBICSPLINE 保留语义，旋转处理四元数单位化/符号和正确插值。VMD 帧号按声明帧率换算（MMD 基线 30 fps），保存每轴位移/旋转的 Bezier；曲线输入时间单调、有界，Bezier 求解有误差与迭代上限，不能把所有关键帧变成线性。重复时刻/空轨道/缺通道、循环端点、负速率和跳转有确定策略。
- FBX/glTF 通道解释为源节点局部变换，VMD 位移/旋转按模型参考骨骼语义转换；缺通道回参考姿态，不把 VMD 位移当世界绝对位置。同骨架直接绑定与跨骨架重定向是两条显式路径。
- Scene 更新一次动画时钟并发布 FramePoseVersion，Renderer 各 Pass 只读同版姿态。普通动画可按帧采样，IK/物理使用固定步及有限补步；Seek/Reset/循环越界重置前帧历史，物理 Seek 从确定快照重演或返回待计算状态，不声称只跳时钟即可得到正确物理。
- 通用顶点/UV Morph 先在参考网格空间组合再蒙皮；Bone Morph 进入姿态，Material Morph 进入实例材质，不修改共享资产。MMD 的变形层级、追加旋转/平移、轴约束、IK、物理前/后骨骼由 adapter 构造有依赖的求解阶段，不能用一个无条件“所有 IK 后全部物理”顺序覆盖全部 PMX。
- Root Motion 单独提取位移/旋转增量，由宿主显式选择原地、交付或应用；动画系统不同时移动 Entity 和重复移动根骨。游戏碰撞/角色控制器可消费增量但不作为预览前置；动作事件遵循时间穿越、循环与 Seek 策略，仅交付事件，不直接发起伤害或其他玩法。

#### 格式支持与适配范围

| 来源/能力 | 目标 | 不隐含的能力 |
| --- | --- | --- |
| OBJ、旧静态 FBX | 维持静态网格行为、场景和 Shader 兼容 | OBJ 不自带骨骼；不因动画引入强制膨胀所有静态顶点 |
| glTF 2.0/GLB | 首个通用动画基准：节点/Skin、inverse-bind、TRS 曲线及 Morph；显式处理材质/嵌入图依赖 | 未支持 required extension 必须拒绝；不因 Assimp 能读就承诺保留全部 glTF 语义 |
| 动画 FBX | Animated 导入保留层级、Skin 与动作片段、明确 take/时间和单位转换 | pivot/pre/post rotation 等无法表示为目标 TRS 时须按带版本/误差的规则烘焙或拒绝 |
| PMX 2.0 与声明的 2.1 子集 | 独立 adapter 保留骨骼/Morph/材质/约束/刚体；BDEF1/2/4，随后 SDEF/QDEF 精确路径 | 当前 Assimp 输出不足以成为完整适配层；2.1 Soft Body 暂列长期扩展，检测并报告，不能宣称完整 PMX 2.1 |
| VMD 动作 | 独立动画资产，骨骼/Morph/IK 启停/模型显示轨道；编码、帧率与 Bezier | 首版版本范围需在 E 开始时固定；相机/光照/自阴影轨道保存但必须显式绑定场景对象后消费，不自动改变编辑器相机或灯光 |
| VMD → FBX/glTF/其他骨架 | 通过参考骨架及 RetargetProfile 映射参考姿态/轴向/长度与根运动 | 仅相同名称不能保证兼容；Morph 需另配目标映射，MMD IK 控制骨不等同目标角色 deform bone |

#### 分步实施与验收

| 阶段 | 交付 | 依赖与验收 |
| --- | --- | --- |
| P18-A 数据与资产契约 | 定义 Model/Skeleton/Skin/Clip 的纯 CPU 数据、导入配置、坐标/版本/依赖与校验，保留静态入口 | 无需 P15；生成两骨/非根 mesh node/矩形网格等小样本，参考姿态、权重/拓扑/非法输入和稳定子资产往返；先不称动画可用 |
| P18-B CPU 姿态与 Scene 生命周期 | 通道采样、参考姿态回退、游标/暂停/循环/Seek、初版组件/Runtime/Root Motion；单次更新发布 | 依赖 A；解析平移/旋转/尺度及不同帧划分结果、Scene Copy/Edit/Play 隔离、保存重载只保存配置、错误依赖保留 |
| P18-C GPU 蒙皮与全部绘制通道 | LBS palette/顶点 ABI、形变法线/切线、动态 Bounds；Color/Shadow/拾取/法线/Toon 外扩共用形变 | 依赖 B；CPU/GPU 参考姿态及两骨运动比对，跨 Pass 同版，移出静态 Bounds 不消失，不同姿态不误实例化；初版蒙皮可不参与静态 Instancing |
| P18-D 通用真实资产闭环 | glTF/GLB 与动画 FBX 导入、稳定 Clip 子资产，最小播放/骨骼诊断 UI | 依赖 A～C；分别验证层级/Skin/动画语义，静态 OBJ/FBX 不退化，播放/暂停/Seek/重载和资产缺失；使用现有 LDR 或无纹理样本，不依赖完整 P15 |
| P18-E PMX/VMD 骨骼核心 | 选择并固定 parser 版本/许可，PMX adapter、VMD Clip/名称绑定/Bezier；兼容矩阵与未支持项反馈 | 依赖 D；启用 Assimp MMD 时更新 Build/Ensure Schema，专用 parser 不越界；中文/日文路径与名称、截断/重复名、坐标/参考姿态、VMD 核心采样比对；不得称 MMD 完整还原 |
| P18-F Morph、MMD 蒙皮与 IK | Vertex/UV/Bone/Material/Group Morph、SDEF/QDEF、追加骨骼/轴约束/变形顺序、IK 限制/启停；MMD Toon/球面贴图/透明/双面/描边适配 | 依赖 E；Morph 组合/组循环拒绝、扭转变形与 CPU/GPU 对比、脚部/膝关节限制、材质实例隔离及遮罩阴影一致性；Flip 等 2.1 项在本阶段按声明范围实现或明确拒绝，Impulse 留给 H |
| P18-G 重定向与通用 Animator | 参考姿态/骨名映射、轴向/比例/根运动、骨骼附件与 Morph 映射；交叉淡入淡出、层/遮罩、加法、Blend Tree/状态图和事件 | 依赖 D/F；同名异层级拒绝、VMD→至少一种非 MMD 骨架、不同体型、循环/切换根运动、附件与多实例无串姿态；可复用通用 IK 做脚部约束，玩法阈值由宿主给定 |
| P18-H MMD 物理适配 | 通用 Physics 最小刚体/关节能力、固定步/碰撞组/骨骼耦合、物理前后求解、Impulse Morph 支持边界 | 依赖 F 和独立 Physics 后端验收；选择后端/单位契约再实现，不假设已有；Reset/Seek/帧划分、骨骼驱动与物理驱动、切场景/复制销毁/发散诊断；Soft Body 不作为首版前置 |
| P18-I 编辑器与资源收口 | 完整时间轴/骨骼/Morph/IK/映射诊断、Graph/Profile Undo/保存；显式相机/灯光轨道绑定、依赖热重载与纹理 Handle/内部资产烘焙 | 依赖 A～H、P15-A/B；不把运行姿态保存为资源；模型/动作/图像导入与发布一致，不复制解码器；材质、Morph、姿态错误有可定位反馈 |
| P18-J 性能、动态速度与发布总验收 | 角色形变 Motion Vector/前帧姿态历史、蒙皮/Morph/物理与动画 LOD 预算、内部资产发布、旧资产回归 | 依赖 I 和 P15 内部纹理运行时验收；无源模型/动作、Assimp/parser/OpenImageIO 也能加载；测目标设备 CPU/GPU/显存与多角色，切换/Seek/首帧速度清零；完成支持矩阵后才验收 P18 |

#### 验证、预算与完成边界

- 各切片先无窗口数据/数学/持久化测试，再真实 GPU 和编辑器流程；采用自有小型样本：单骨/两骨、非根 mesh、不同长度/参考姿态/命名的两骨架、带 Morph 的网格、简化腿部 IK/刚体关节。真实 glTF/FBX/PMX/VMD 样本必须能说明来源、预期与格式范围，不以截图正常代替采样/求解验收。
- 候选门槛：CPU/GPU 顶点误差 ≤1e-4×max(模型包围盒对角线,1)，单位法线角差 ≤0.1°；曲线/重定向/IK 另用解析与固定参考数据定门槛。实施阶段在目标设备确认，不提前登记 PASS；非法骨架、超限、失配/缺依赖、未知枚举和复杂格式降级必须可见。
- 预算单列每实例当前/前帧姿态、palette、Morph 稀疏/密集数据、Bounds 和物理状态，不与共享网格/贴图混算。候选 32/128/256 骨、1/16/64 角色分别测 CPU 求值与 GPU Color/Shadow/形变成本，普通帧禁止 GPU 读回驱动动画；首版不承诺任意骨数或角色数量。
- Color/Shadow/拾取/法线与透明/Mask/Toon 多 Pass 采样同一姿态/Morph；不能沿用静态 Bounds 或相机 Velocity 声称完整动态结果。没有蒙皮 ABI 的自定义 Shader 要拒绝/诊断，不让蒙皮模型被静态 Shader 无声绘制。
- 重定向并不使所有 VMD 兼容所有模型；非人形动画、复杂 MMD 扩展/PMX 2.1 Soft Body、PMD、BVH、USD、布料、完整 DCC 图编辑与物理网络同步列为后续扩展，不隐含于首版。支持矩阵逐项记录解析/绑定/运行/渲染/发布五层能力；“可解析”不代表“可播放”或“与 MMD 一致”。
- P18 只建设通用动画与格式适配，不内置战斗/移动 AI/任务状态；示例 Animator 状态图属于测试资产。Root Motion 的碰撞处理、游戏规则、角色选择与行为由宿主实现。

**参考与核验依据**：当前源码 ModelImporter/AssimpModelImporter/MeshSource、AssetType、Renderer3D/ShadowRenderer、Scene/SceneSerializer 与 Shader ABI；本地固定版 Assimp MMDImporter/MMDPmxParser/MMDVmdParser 和 Build/Ensure 配置。[glTF 2.0 官方规范](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)用于节点/Skin/动画插值契约，[Khronos 蒙皮示例](https://github.khronos.org/glTF-Tutorials/gltfTutorial/gltfTutorial_020_Skins.html)用于参考空间校验；[MMDFormats 源实现](https://github.com/oguna/MMDFormats)仅作为 parser 候选与行为参考，不自动引入依赖，实施时审查版本、覆盖范围和许可。

### 长期候选

- Vulkan 后端实际实现，而不只是接口预埋：先统一 RendererAPI/Buffer/VertexArray/Texture/Shader 后端创建入口，再建立 Context、Surface、Swapchain 与帧同步，以及 Render Pass/Pipeline/Descriptor/Command Buffer 映射；定义 GLSL→SPIR-V 编译、反射布局、版本化缓存与热重载失败回退。该候选尚未开始，不改变当前 OpenGL 主线；
- GPU Driven Rendering、遮挡剔除和间接绘制；
- 完整三维大气与体积流体；
- Linux/macOS 应用图标和打包资源；
- 统一 Asset Command、Dirty 状态和保存提示；
- 发布构建、资源打包与项目模板。

## 已完成里程碑

### 2026-10-07：水面最终方案 W1～W4 与集成验收

- 用户本轮明确提升完整水面建设优先级；专项已完成，当前主线恢复 P17-C3。交付为已有 Terrain GPU 水文的通用高度场水面，未建设游戏河流规则、独立水体模拟、FFT 海洋或三维流体。
- **W1**：公共 WaterSampling GLSL 显式双线性读取 Height/Water/Velocity/Sediment，逐节点有限值保护，各自尺寸/端点与 X/Z 物理间距；地形瞬时岸线湿润复用连续采样。Compute/SimulationGrid、模拟水量与分辨率未改。
- **W2**：WaterSurfaceAppearance v1 保存外观、质量及稳定法线/噪声 Handle；旧场景缺字段使用默认，未知 schema/档位与非有限/越界值在保存/加载预检拒绝。Inspector 专用水面命令与连续事务支持 Undo/Redo，只改 Water，不失效地形或 Reset。默认内置 256² 周期 Fourier 法线/噪声，两个有界流动相位与旋转第二尺度，GL/DX 方向转换、浅水/远景衰减、完整 Mip；外部 Linear Normal/Data 纹理通过现有资产身份加载和轮询，受控失败保留/回退、修复恢复。没有引入第三方贴图文件或许可依赖。
- **W3**：速度泡沫与浅水岸线泡沫由噪声调制，覆盖以像素导数抗锯齿，干格仍剔除；吸收、折射、粗糙度、泥沙、湿润参数持久。保留 HDR/Depth 背景快照、前景拒绝、深度/拾取/单位法线与诊断绕过。
- **W4 最终取舍**：复用 Chunk 布局但独立缓存统一 32/64/128 格 Mesh，单个 Terrain 全块同档、不随地形 LOD 变化，边界全局 UV/位置一致；避免跨级裂缝与相机跳变，保留旧路径作为会话对照。大范围自适应 LOD、逐块干水剔除和高度上界另立优化，不把统一质量当作 D/Morph 已完成。
- **职责/生命周期**：Simulation 唯一写入物理场；引擎渲染与纯规格/资产接口负责水面，编辑器负责保存外观及预览，游戏只选择外观与是否模拟。共享背景、内置、最多三份 Mesh 和自定义用途缓存归 Renderer，暂停/关闭绘制仍计驻留，Shutdown 释放。首次可选 Water Shader 失败跳过，后续失败保留旧 Program，修复恢复；首次无 Program 按秒重试。解码失败返回空且不缓存失败；Opt-in Mip 分配/生成与奇数 RGB/R8 行对齐修复已接入。
- **构建/数值证据**：VS2026 Debug x64 全解决方案完全链接构建、285 条无窗口 PASS 通过；场景往返、缺字段、非法 schema/数值、复制与预设保留通过。Intel Iris Xe / OpenGL 4.5/4.6 的本地水面 GPU Contract 全部通过：5×7 解析平面/端点最大高度差 `2.98023e-8`、法线差 `6.15871e-8`；NaN/Inf、三档网格、跨块位置/UV、镜像非均匀 Transform、Mip/奇数行、暂停/固定步/Reset/帧划分、缺/损坏/错误语义贴图及首次/热重载 Shader 失败/修复、前景折射/拾取/重叠顺序、Resize 与绘制前后 Water 不变通过。
- **编辑与兼容证据**：实际 ImGui 水面开关与连续 Slider Undo/Redo 不改变表面版本/Height 引用/Dirty；原印章/查询/模式/生命周期集成通过。原水文/气候与 512/1024/2048 地形 GPU Contract 保持 PASS，水质量相对误差 `9.83321e-7`；空配方 v1/v2 BMP 与 A2 基线逐字节一致。静态冷启动 Water 0 Draw/0 驻留，远景、近景反射波纹、粗采样对照和三档水面截图已检查。
- **成本/验收门槛**：本机 1024² 模拟、WorldSize 1024、16 可见块、558×353 静止水面 Fixture，10 帧预热后至少 30 个可用异步计时样本。Low/Balanced/High 分别 32,768/131,072/524,288 三角形，Copy 平均 0.078/0.078/0.073 ms，Draw 平均 1.407/2.475/3.924 ms；Balanced 复制+绘制均值 2.553 ms，High 3.997 ms。本次依据实测设该 Fixture 的 Balanced ≤3 ms/High ≤5 ms 均值门槛，均通过；不外推整帧 FPS 或高分辨率/8192 范围性能。
- **资源证据**：当前背景 2,363,688 字节，内置贴图含 Mip 349,524 字节；每档 Mesh 56,952/212,088/817,272 字节，单档 Balanced 驻留合计 2.790 MiB（门槛 ≤3 MiB），High 3.367 MiB（≤4 MiB）。启动视口 Resize 的保守旧+新跟踪峰值 Balanced 21,758,412/High 22,363,596 字节，均低于本次 24 MiB 门槛；它是 Renderer 资源账本，非驱动实测显存，不含模拟/CPU/Shader，三档缓存累积另按实际计数。没有新增全尺寸视觉场。
- **后续边界**：独立任意水体、FFT/几何波浪、SSR/水下透明折射、多层透射、CSM 水面投影、持续湿润、Reactive Mask/水流 Motion Vector 与按屏幕误差水体 LOD 均未实现；细分仍限制近岸轮廓，保护图仍不约束运行时侵蚀。这些不隐含在本轮水面范围或完成结论内。
- 三文档已同步、差异检查通过；验证日志/图片和本地 Contract 留在忽略的 bin，不新增独立报告或图片归档。提交：待提交。

### 2026-10-07：水面视觉优化方案完善（仅文档）

- 完成 W1 连续视觉采样、W2 波纹法线、W3 泡沫/岸线及条件 W4 几何优化的职责、资产/时间契约、实施顺序与验收设计；当前主线仍为 P17-C3，所有 W 切片未实施。
- 源码核对 TerrainHydrologyGPU、WaterSurfaceRenderer、WaterSurface.glsl；确认 Nearest 模拟纹理、共享地形 LOD、正弦波纹及当前会话参数边界，未将用户网格感主因记为已定位。
- 三文档同步审查与差异检查；本轮未改代码/资源，未运行构建或 GPU 验收，不新增运行通过结论。提交：待提交。

### 2026-10-07：P17-C2 地形资源与帧内表面生命周期

- 引擎新增 TerrainRenderer::BeginFrame，Scene Runtime/Editor 在阴影前开始地形帧；第一次 Prepare 完成生成/模拟更新，Runtime.PreparedFrameSerial 固定本帧发布结果，颜色 Prepare 不重复更新，水面直接消费已准备高度。帧内规格修改/Invalidate 延后到下帧，帧外 Prepare 仍即时处理；修复阴影先读旧高度、颜色随后推进模拟的时序差异。颜色 BeginScene/EndScene 计时边界保留，模拟提前到计时外，C3 必须按分项核对成本。
- 修复 Reset+Step 同帧且关闭侵蚀时派生图刷新标记被覆盖的问题，按 Reset→Seed→Step 处理并保留 Reset 刷新要求。生成 Shader 失败热重载也触发发布校验并显示 GenerationError，完整旧表面/版本/模式/模拟资源保留，修复成功后重新发布；模拟 Shader 首次加载和设备故障仍不属于保证。
- 验证：标准 Debug x64 全解决方案完全链接构建和 273 条无窗口 PASS；Intel Iris Xe GPU 生命周期闭环 PASS。真实 BeginFrame→阴影 Prepare→BeginScene→颜色 Prepare 时序固定高度/版本，重复 5 次不重复 Step，帧内模式修改下帧生效；Static 排队 Step/Reset/Seed 不补执行。Static 81²/Simulation 97² 与 WorldSize=1536 的 Resize、保存重载、Scene Copy 身份隔离、导入往返与模式资源恢复通过；Reset+Step 后 Height/Normal 与初值一致且仅一次派生刷新；两模式生成 Shader 成功/失败/修复热重载通过。
- 原印章 GPU Contract 高度/保护最大差 6.258488e-7 / 5.960464e-8，512/1024/2048 采样/派生 Contract 与表面哈希 12094008686276857432 保持；水文/气候 Contract PASS，静态查询最大差仍为 1.7881393e-7。静态/模拟固定视角已捕获检查，Color 16 Chunk、Shadow 4 Cascade/3 Draw，Water 分别 0/16 Draw；空配方 Data v1/v2 与 A2 基线逐字节一致。临时 Shader 为独立副本并自动清理，未改原 Shader 或注册临时资产；图片/日志仅本地 bin。
- 三份文档已复核同步；C2 从活动清单移出，当前提升 C3 的资源清单/重建峰值/分项性能复核；不完成 C 整体，不据本轮正确性验证宣称性能预算达标。提交：待提交。

### 2026-10-07：P17-C1 显式静态/模拟模式与兼容持久化

- 引擎 TerrainExecutionMode 提供 Static/Simulation；新规格默认 Static，Scene YAML 数值 0/1，缺字段按旧 Simulation 加载，未知值在保存/加载预检及 Prepare 时拒绝。预设和 Scene Copy 保留模式，Inspector 顶部选择通过整份规格命令支持 Undo/Redo，展示已发布模式；Debug 说明全局模拟控制仅影响 Simulation 地形。
- Static 直接使用 Generator 高度/派生/遮罩，不创建 GPUHydrology/GPUClimate/GPUEnvironment 或 CPU Hydrology；成功切回 Static 释放模拟对象，导入高度图仍不模拟。切换重建表面并使旧快照过期，Simulation 从独立静态初值初始化并支持 Reset；初始化时消费旧请求序号，避免补执行 Static 期间的 Step/Seed。受控失败保留原模式、表面及资源；本轮未扩展模拟 Shader 首次加载/驱动故障恢复保证。
- 验证：标准 bin/Debug-windows-x86_64 目录 VS2026 Debug x64 全解决方案完全链接构建与 273 条无窗口 PASS，通过新默认/显式持久化、旧字段回退、未知值保存/加载拒绝。首次验证代码的 const 限定编译错误已修复并完成重建。Intel Iris Xe GPU 集成通过 Static 冷启动及重复 Prepare 无模拟资源、Inspector 模式命令 Undo/Redo、两方向失败切换保留、静态快照过期、模拟初值/Reset 和 Copy 隔离；原编辑/重载/Resize/保护/查询闭环保持 PASS，查询最大归一化差 1.7881393e-7。
- 静态固定视角已检查，Color 16 Chunk、Water 0 Draw、Shadow 4 Cascade/3 Draw；原水文 GPU Contract PASS（水量相对误差 9.83321e-7，侵蚀质量误差 2.58287e-7），气候 GPU Contract PASS；空配方 Data v1/v2 图像与 A2 基线逐字节一致。原印章/采样/派生 Contract 同样 PASS，高度/保护最大差 6.258488e-7 / 5.960464e-8，表面哈希保持 12094008686276857432。图像/日志仅本地 bin；本轮使用标准启动产物，旧 review 目录程序不会随构建更新。
- 三份文档已复核同步，C1 从活动清单移出，当前提升 C2。C2 进一步生命周期组合验证与 C3 完整纹理清单/峰值/性能复核尚未验收，不完成 C 整体；提交：待提交。

### 2026-10-07：P17-B3 创作诊断与 B 阶段验收

- Inspector 新增 Terrain Authoring Diagnostics：全局保护/裁切视图、已发布身份/生成版本、显式静态快照捕获及局部 XZ 高度/法线/坡度查询、显式裁切节点计数。快照与计数在重建后标为过期，不自动回读；场景/地形身份切换清空诊断缓存；失败发布保留旧表面的有效查询。仅 Ready 展示查询数值，不夹取越界坐标。
- 引擎 Renderer 读取已发布保护/裁切纹理用于片元诊断，复用采样槽 23，无新纹理或生成 Dispatch；诊断时水面绕过绘制，原设置保留，拾取与法线输出保留。模式和 Inspector CPU 缓存均不进入 Scene/YAML 或 Undo。游戏规则、实际细节与侵蚀保护未接入；查询仍限程序化 Data v2 静态表面。
- 验证：VS2026 Debug x64 全解决方案完全链接构建成功，267 条无窗口 PASS；本地产物 bin/P17-B3-review。隔离 ImGui 实际 Capture/Count 控件通过，无自动捕获、重建过期、越界、失败保留、场景重置及视图隔离通过。Intel Iris Xe 编辑闭环验证 Undo、保存重载、Play 副本保护图内容一致；145×145 Resize 全节点保护与 CPU 参考一致，受控失败保留纹理引用及字节；原快照最大归一化差 1.7881393e-7。
- 65×81/129×145 印章 GPU Contract PASS（高度最大差 6.258488e-7，保护最大差 5.960464e-8），512/1024/2048 采样/派生 Contract PASS，组合表面哈希保持 12094008686276857432。保护及独立裁切案例 BMP 已捕获并检查；诊断 Color 16 Chunk、Water 0 Draw、Shadow 4 Cascade/3 Draw。普通 None 视图 Water 16 Draw；空配方 Data v1/v2 图像与 A2 基线逐字节一致。
- 三份文档已复核并同步；B3 从活动清单移出，B 整体验收完成，当前项提升 C，C 尚未实现。图片/日志仅本地 bin，不新增项目归档；提交：待提交。

### 2026-10-06：P18 模型/动作完整适配方案与 P15 依赖拆分

- 用户要求仅完善文档；新增通用骨骼/动作、glTF/FBX、PMX/VMD、Morph/IK、重定向、Animator、物理、编辑器及内部资产发布的 A～J 实施/验收方案。确认 P15 全量不是动画核心前置，模型纹理收口与发布复用其最小解码/烘焙能力，EXR/Cubemap 不阻塞动作播放。
- 核验现有静态导入与 Renderer/Scene 边界、Assimp 固定版 PMX 实际入口/VMD parser 状态，并对照 glTF 官方节点/Skin/动画契约。三份文档已复核：本文件保存设计与依赖，ARCHITECTURE/README 只补当前已实现与缺失边界，不将计划记为架构事实。
- 验证为源码/依赖与文档静态检查、阶段/依赖/状态去重及 git diff --check；无代码修改，未运行构建/GPU。只完成方案，P18-A～J 与 P15 切片均未实施；唯一当前主线仍为 P17-B3；提交：待提交。

### 2026-10-06：P17-B2 显式 CPU 静态地表快照与版本化查询

- 引擎 TerrainSurfaceSnapshot 保存独立 CPU 高度数据、尺寸/范围/HeightScale 与表面版本；局部 XZ 闭范围内按端点网格双线性插值及解析梯度返回高度/法线/坡度。未就绪、过期、非法坐标/数据和越界显式反馈，不夹取查询坐标；错误初始化保留旧快照。内节点取正向单元，外端点取最后单元，导数可能不连续；与 LOD Mesh 三角化、派生法线插值、碰撞和导航严格区分。
- Renderer 显式同步捕获已发布 Data v2 程序化 Generator 的组合后静态 R32F 高度，不调用 Prepare、不自动缓存/刷新，普通帧无新增回读。旧 Data/导入图明确返回 UnsupportedSurface；不读取水文动态 Height，不隐含世界 Transform。每份 Runtime 使用独立非持久身份及 GenerationVersion；生成失败保留旧表面与查询有效性，成功重建/导入使旧版本过期，重载/Play 副本不能混用。游戏规则和坡度通行标准由宿主拥有。
- 验证：VS2026 Debug x64 全解决方案构建与 267 条无窗口 PASS 输出通过；构建位于本地 bin/P17-B2-review，保留占用标准 exe 的原编辑器。解析矩形网格斜面、双线性非平面、端点/越界、非有限输入、零 HeightScale、非法替换与身份/版本反馈均 PASS；未准备捕获不创建 Runtime。
- Intel Iris Xe 编辑器 GPU 集成 PASS：平台核心高度/零坡度、145×145 全节点静态高度查询（最大归一化差 1.7881393e-7，小于 1e-5）、法线单位长度/有限坡度、重建过期、重载/Play 身份隔离、Resize 范围、失败保留、首次失败、导入及 Data v1 拒绝。模拟高度清零后捕获静态表面不变，Reset 恢复，原 Inspector/Undo/保存/重载/Play/失败恢复闭环保持 PASS。
- 空配方 Data v1/v2 固定视角 BMP 与 A2 原基线逐字节一致；印章表面捕获并检查，Color 16 Chunk、Water 16 Draw/背景快照就绪、Shadow 4 Cascade/3 Draw。快照与查询没有新增可视面板，日志/截图仅本地 bin；三份文档已复核并同步，B2 从活动清单移出，当前项提升 B3，B 整体尚未验收；提交：待提交。

### 2026-10-06：P17-B1 独立保护遮罩契约与输出

- CPU 配方评估返回 ProtectionWeight，以轮廓/过渡×Strength 按 P=max(P,w) 合并；GPU 在既有印章 Dispatch 内输出独立 R32F 保护图。保护权重与裁切、高度增量和合成顺序分开，零高度增量也能保护；空列表/全部禁用或零强度不分配、不增加 Dispatch。解析参考校验有限输入与 P∈[0,1] 后计算 residual×(1-P)，尚无实际细节或侵蚀保护消费者。
- Generator 将保护图随完整候选统一交换，Renderer 发布 Runtime 引用，受控失败保留旧图；Resize 发布匹配尺寸图，成功导入清空引用。保护图为静态创作结果，不由模拟更新；配方 YAML 与版本 1 不变。有效印章的原始生成纹理为约 40 字节/节点（Height 双缓冲/三派生图 32，加裁切/保护各 4），候选峰值与模拟成本仍未优化。
- 验证：VS2026 Debug x64 全解决方案构建与 255 条无窗口 PASS 输出通过；标准 exe 仍被原编辑器占用，完全链接产物位于本地 bin/P17-B1-review，保留原窗口。CPU 解析用例覆盖核心/过渡/外部、旋转/平移、max 而非累加、重排、禁用/零强度、零增量/裁切独立、正负残差与非法输入。
- Intel Iris Xe GPU 的 65×81/129×145、1/3/64 操作 Contract PASS；保护最大差 5.960464e-8，高度最大归一化差 6.258488e-7，均小于 1e-5。逐节点有限范围、并集顺序无关、零增量高度/派生不变、分数强度、解析残差、重复生成确定性、非法配方/缺失与编译失败 Shader 后保护引用和内容保留均通过。实际 Inspector/GPU 编辑闭环及新增保护图 Resize/失败保留/导入清理通过。
- 原 512/1024/2048 采样/派生 Contract PASS，表面哈希仍为 12094008686276857432；空配方 Data v1/v2 固定视角 BMP 与 A2 原基线逐字节一致。印章表面捕获并检查，Color 16 Chunk、Water 16 Draw/背景快照就绪、Shadow 4 Cascade/3 Draw；没有据此宣称遮罩可视化或持续侵蚀保护已完成。日志/截图仅本地 bin。
- 三份文档已复核并同步；B1 从活动清单移出，当前项提升为 B2，B 阶段整体尚未验收；提交：待提交。

### 2026-10-06：P17-A3 印章编辑、事务与渲染集成；P17-A 验收

- Inspector 增加 Terrain Stamps：Rectangle/Ellipse、Add/SetHeight、启用、局部中心/核心尺寸/旋转/过渡/强度/高度，以及增删/重排/清空。稳定 ID 维持控件身份；编辑器 helper 只操作规格，GPU 合成归引擎。离散操作单命令，连续编辑激活/释放提交整份规格快照；Undo/Redo 与属性修改通过 Invalidate 保留旧 Runtime，预设不擦除配方。
- 修正 Procedural/分辨率编辑直接清空 Runtime 的旧行为；导入高度图先解析候选，缺资产保留旧表面，成功切换清理程序化模拟资源。Inspector 显示校验/生成错误。新增会话级灰模预览保留几何法线、光照和阴影；仅 recipe fixture 环境变量即可手动打开持续运行的场景，水深注入只属于编辑器验证数据。
- 验证：VS2026 Debug x64 全解决方案构建与 243 条无窗口 PASS 输出通过；原 exe 被之前的运行实例占用，因此使用独立 bin/P17-A3-review 完全链接构建，保留原窗口。实际 Inspector 控件在隔离 ImGui 上下文直接激活，Add/Enable/Reorder/Remove/Clear 及连续高度激活/释放 Undo/Redo PASS。GPU 编辑闭环验证新增/拖动/重排、保存重载、Play 复制隔离、129→145 Resize/WorldSize、非法配方/高度/缺 Shader/首次失败的完整旧资源/版本/模拟状态保留与恢复、程序化/导入切换，以及静态初值与修改模拟高度后的 Reset。
- Intel Iris Xe 上 A2 CPU/GPU Contract 仍 PASS，最大归一化高度差 6.258488e-7；原 512/1024/2048 采样/派生 Contract 及重建哈希 12094008686276857432 保持。固定视角灰模/材质/静止水面已捕获并检查：Color 16 Chunk，Water 16 Draw 且背景快照就绪，Shadow 启用、4 Cascade/3 Draw。空配方 Data v1/v2 与 A2 修改前基线逐字节一致；截图/日志仅本地 bin，不归档进项目。
- 三份文档已复核并同步；A1～A3 共同完成 P17-A。未声称模拟中持续保护、移动 LOD 连续 Morph 或性能预算已验收；这些分别留在 B/C/D/F。当前主线提升为 P17-B1，提交：待提交。

### 2026-10-06：P17-A2 GPU 印章合成与候选表面发布

- TerrainGenerator 在 Thermal 后、Derive 前接入独立 ApplyTerrainStamp Compute；Ellipse/Rectangle、Add/SetHeight 按列表顺序执行，每个有效操作一个 Ping-Pong Dispatch，Barrier 后交换；空列表、禁用/零强度维持旧输出与 Dispatch。CPU/GPU 使用同一端点/局部单位契约，派生图和模拟初值消费最终组合高度。
- GPU R32F 裁切标记延迟分配，只在显式诊断或验证模式读回。Generate 验证配方和 Shader 后生成完整候选 Height/派生图，成功统一发布；Prepare 对分辨率/Shader 变化持有 PendingGenerator，失败保留已发布表面、规格和版本。Color/Shadow/Water 使用已发布规格；错误可从 Runtime.GenerationError 读取，显式 Invalidate 或成功热重载触发重试。
- 验证：VS2026 Debug x64 全解决方案构建及无窗口回归通过，235 条 PASS 输出；Intel Iris Xe / OpenGL 上 65×81 与 129×145 GPU Contract PASS，覆盖 1/3/64 个印章、重叠顺序、旋转/过渡/端点裁切、禁用/零强度、确定性、未知版本、缺失/编译失败 Shader 的输出保留与恢复、模拟初值/Reset。最大归一化高度差 6.258488e-7，门槛 1e-5；512/1024/2048 原采样/派生 Contract 与最终表面重建一致性通过，2 个印章 Fixture 32 Dispatch、组合哈希 12094008686276857432。
- 同视角空配方 Data v1/v2 BMP 与修改前逐字节一致，SHA256 分别 BFC65B781458B1D9237F4449A09B7039C5A03685ADF5F43D0A9EDE3ECB3DC065 / 8A771E0D2D889D5985CCD11C3089C3D9A2E1D56CA5B7903AA8DB1D57575DA817。日志/截图仅存本地 bin。首次构建缺 ShadowRenderer include 已修复；验证中发现增量链接保留旧验证代码，强制完全链接后确认编译失败用例与最终数值输出实际运行。
- 三份文档已复核并同步；本切片实现通用引擎合成与中性验证场景，没有任务/撤离等游戏功能。Inspector、实际 Resize/失败的编辑器流程及灰模/阴影/水面专项验收留给 A3，不完成 P17-A。提交：待提交。

### 2026-10-06：P17-A1 配方数据、CPU 参考与持久化

- 在引擎 Terrain 层新增 TerrainRecipe/TerrainStamp 和结构化校验；支持 Ellipse/Rectangle、Add/SetHeight、有序操作、稳定 uint64 ID、局部单位与外侧平滑过渡。CPU 参考使用双精度中间值避免有限极端参数溢出，按既有高度范围限制并报告 Clipped；矩形距离/椭圆径向度量的区别已明确。
- TerrainSpecification 保存配方，基础来源/Seed 复用原字段。非空 Recipe 经 Scene YAML 保留全部字段及顺序；空配方不输出新字段。未知版本/枚举或非法配方拒绝保存，保留原字符串/文件；加载前预检全部地形，配方失败不向目标 Scene 添加实体。整体组件命令/Scene Copy 保持配方值语义和 Runtime 隔离，预设切换保留配方。
- 验证：2026-10-06 `scripts/Verify-Windows.ps1` 完成 VS2026 Debug x64 全解决方案构建与无窗口回归，日志共 233 条 PASS 输出；新增解析核心/过渡、旋转/平移、重叠顺序、局部尺度、禁用、裁切、极值与错误索引、容量/版本、持久化、未来版本拒绝、失败保存、场景隔离和 Undo/Redo 用例通过。初次沙箱构建因 SDK 目录访问拒绝失败，扩大权限后通过；保留既有 Instrumentor/Renderer2D/strncpy 编译警告。
- 三份文档已复核并同步：ARCHITECTURE 描述已落地的数据/所有权/加载边界，README 记录 CPU API 与当前限制。本切片未改变 GPU 生成链/地形外观，未运行 GPU 数值/视觉验收，不完成 P17-A；当前实施项提升为 A2。提交：待提交。

### 2026-10-03：通用分层地形方案与主线重排（仅文档）

- 按用户要求完成可实施方案，本轮不改代码。核实清理后基线为 P16-A，未保留 B1 Tile；保留已验收实现，旧 P16 未完成目标明确并入 P17 或降为可选扩展，不标为完成。
- 当前唯一主线改为 P17，下一实施项为 A：配方/解析印章 CPU 契约与持久化回归，随后 GPU 合成及 Inspector/Undo/真实渲染验收。方案提供职责边界、数据/版本/单位与排序契约、失败发布、模拟初值、分阶段交付及预算/验收。
- 明确游戏宿主负责地点含义、任务布局和玩法规则；引擎只提供通用地形组合及查询，编辑器只负责创作入口与事务。碰撞/导航/Prefab 不假设已有，不作为首版印章能力的前置。
- 验证：静态核对 TerrainSettings、TerrainSampling、TerrainGenerator、TerrainRuntime、TerrainRenderer、Scene/Components、SceneSerializer 与 Chunk 数据流；文档阶段/依赖/历史去重及 diff 检查。仅方案完成，P17-A～F 均未实现或验收，未运行构建或 GPU。
- ARCHITECTURE 与 README 已复核：本轮没有实现结构或用户功能变化，两份不新增计划文字；方案和候选预算只保存在本文件。提交：待提交。

### 2026-09-30：P16-A 尺度与分层契约

- A1：新增引擎层 `TerrainSampling.h`，区分端点节点和单元中心的间距/位置/纹素 UV；Data v2 程序化 Height 使用端点布局，Color/Shadow/Water 共用采样换算。当前 Runtime 模拟保持原分辨率和节点布局，未分配局部 Tile。
- A2：DataVersion 与 SynthesisVersion 分离，新规格默认 Data v2，旧 YAML 缺字段固定 Data v1；预设保留版本，编辑器切换和坡角参数参加既有配置/Undo 链。v2 使用物理梯度坡角、凹地正曲率和世界距离稳定坡角阈值；修正热蚀边界重复邻居搬运。旧 Data v1 行为保留。
- A3：实现 Base+Detail+(Current-Initial) 局部高度组合和 Ping-Pong/Halo 原始纹理预算工具，并验证 Delta 只相加一次。生成器/水文/气候所有权保持不变；DataVersion 改动经既有失效/重建流程刷新派生和模拟初始状态，场景复制只复制规格，Runtime 不序列化。此项是契约交付，局部细节资源、缓存、LOD Morph 和模拟降采样尚未实现。
- 验证：`scripts/Verify-Windows.bat` Debug x64 全解决方案构建与 193 项无窗口 PASS 输出通过；解析场覆盖三档端点/坡角、碗形/凸丘、稳定坡角、斜邻居、零幅度/非有限值、旧场景往返、Undo 和资源预算。Intel Iris Xe / OpenGL 4.6.0（驱动 32.0.101.6790）实际 GPU 三档斜面及边缘法线误差 <0.002、坡角误差 <1°；二次曲率符号/土壤权重、34° 面保持、角点热蚀质量误差 <1e-5、零幅度检查通过。五预设 × 三分辨率的有限性/范围/法线及四层权重检查通过。
- Alpine 1024 Data v2 重复生成哈希 `1222385975937983075`；Data v1 与本次改动前的三张 Compute Shader 对照哈希均为 `12660624032591812628`，固定截图 SHA-256 均为 `BFC65B781458B1D9237F4449A09B7039C5A03685ADF5F43D0A9EDE3ECB3DC065`。哈希仅作同机同驱动证据，不要求等于历史驱动值。水文/气候 GPU 契约及固定视角截图通过，未以此宣称河网或近景细节已完成。
- 基线：Seed=1、WorldSize=1024、五预设默认参数及热侵蚀，Data/Synthesis=2；下面为归一化 Height 范围，完整 hash/spec 由 `GLIMMER_TERRAIN_VALIDATE=1` 的基线日志输出。

| 预设 | HeightScale | 512² | 1024² | 2048² |
| --- | --- | --- | --- | --- |
| Alpine | 168 | 0.081573～0.608876 | 0.081489～0.619971 | 0.081659～0.654059 |
| Plateau | 120 | 0.138131～0.482104 | 0.138105～0.482870 | 0.138095～0.482465 |
| Rolling Hills | 64 | 0.052844～0.316829 | 0.052830～0.316848 | 0.052828～0.316848 |
| Volcanic | 152 | 0.044158～0.779133 | 0.044922～0.779866 | 0.045143～0.779827 |
| Eroded Valley | 136 | 0.081761～0.502787 | 0.081598～0.541656 | 0.081467～0.564809 |

- 本轮已复核并同步 PROJECT_STATUS、ARCHITECTURE、README；当前实施项提升为 P16-B/B1，P16 总体仍进行中；提交：待提交。

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

### 地形创作与组合

- P17-A/B 已具备配方数据/持久化、CPU/GPU 合成与保护权重输出、Inspector 创作及遮罩/查询诊断、版本化 CPU 静态地表查询与静态三通道集成验收；旧 Data/导入/动态模拟查询适配、实际细节/侵蚀保护消费者、完整成本及阶段总验收、局部缓存与连续几何仍缺失。当前全局候选发布增加重建峰值纹理成本，64 操作仅验证正确性，未验收性能预算；设备丢失/驱动分配错误恢复不在受控失败保证内。
- TerrainRenderer::Prepare 同时承担生成资源创建与环境模拟协调；P17 先扩展生成器的组合契约，再于 C 收拢生命周期，避免在 A 混合重写渲染器。Simulation 中暂停仍分配全尺寸水文/气候纹理，Static 已不创建模拟对象；C2 已完成生命周期组合验证，C3 继续核对完整资源清单与成本。
- 当前高度 Bounds 基于 HeightScale，印章不可未经更新 Bounds 就输出任意局部高度；查询必须区分高度场插值与离散 LOD Mesh。碰撞、导航与玩法可达性尚无对应正式系统验收。

### 构建与依赖

- VS2026 Premake 生成的主入口是 `GlimmerEngine.slnx`；旧 `GlimmerEngine.sln` 可能来自 VS2022 或早期生成，自动验证脚本优先构建 `.slnx`；
- GLFW、Glad、ImGui、yaml-cpp、ImGuizmo 与 SPIRV-Cross 的 Premake 项目由主仓库 `scripts/premake/Dependencies.lua` 统一定义；子模块只提供源码，不应再写入本地 Premake 或 Visual Studio 工程；修改适配层时必须复验 SPIRV-Cross CLI/samples/tests 排除规则；
- Assimp 是独立生成且不提交产物的静态依赖；Glimmer 的 PreBuildEvent 与 `Verify-Windows.ps1` 会检查产物、构建 Schema、配置、子模块提交和 ccache 状态，过期时自动调用 Ensure/Build 脚本。仍可手动运行 `scripts/Win-BuildAssimp-vs2026.bat Debug|Release` 强制重新配置依赖。
- `assets/models/Cerberus` 已提交约 175 MiB 的 FBX/TGA 测试资源，但仓库缺少原许可说明；公开分发或商业使用前必须补齐明确的再分发许可，否则应从发布资产与版本化回归中替换为自有小型样本。
- Model 资源已恢复 Cube、Plane、UV Sphere、bunny、planet、spacecraft、suzanne；注册表中的 `models/New Folder/Cube.obj`、`models/dragon.obj`、`models/UV Sphere.obj` 仍缺少源文件，需要从原设备恢复或移除失效条目。
- FBX 当前仅支持静态网格并把节点变换烘焙到顶点；尚无单位归一化、保留层级、骨骼/动画、Morph Target、嵌入纹理、自动 `.glmat`/`.glmesh` 烘焙或 glTF/GLB 注册。PMX/VMD、通用骨骼/蒙皮/Animator、跨骨架重定向与角色物理均未实现，建设范围/依赖/格式支持矩阵集中在 P18；模型纹理资产化复用 P15，不因新增计划宣称任一能力已可用。

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
- P16：Data v1 保留旧 Talus、曲率与材质解释；Data v2 尺度/符号问题已在 P16-A 修正并验收，但热侵蚀搬运速率和迭代次数仍不保证分辨率无关。Analysis Flow 只是局部下坡量，不是上游汇流累积；Normal/Analysis/Weight 在顶点读取后插值，近景精度受网格约束。局部 Tile 资源、跨块模拟边界与连续几何过渡尚未实现；
- CSM 已完成 Practical Split、Texel Snap、可调重叠混合、基于 Bounds 的 Shadow Frustum 剔除、运行时级联着色、Alpha Mask 投影和每级 Model Instancing；Terrain 仍独立提交，Blend 默认不参与 Shadow Pass，尚无彩色透射或抖动式半透明阴影；
- SkyLight 已支持六面 LDR/等距柱状 HDR、线性 `RGBA16F`、完整普通 Mip Chain、内存派生缓存，以及 Model/Terrain 共用的 Diffuse Irradiance、GGX Specular Prefilter 和 Split-Sum BRDF LUT；尚无持久化磁盘缓存、环境旋转、局部 Reflection Probe 或动态场景反射；
- P14 已完成 CPU/GPU 场、TerrainRuntime 所有权、固定步调度、四场诊断、GPU 趋势 Contract，以及 Rainfall/Evaporation 到 P13 Water 的守恒耦合；水面/瞬时岸线视觉基础已接入；气候材质权重反馈和植被实例化仍未实现；
- Water Surface W 已完成连续视觉采样、周期法线/泡沫、持久外观和独立统一细分；水文原场仍为 Nearest。尚无屏幕误差自适应水体 LOD/逐块水量剔除、水下透明折射、分层透射、SSR、水面阴影、持续湿润或水流 Motion Vector；统一细分仍限制近岸轮廓，大范围/高视口性能不由本次 1024/558×353 基准保证。空水域仍提交表面 Draw 后在片元剔除；C3 全地形/模拟资源与峰值尚未验收。
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
