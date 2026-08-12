# Glimmer 项目状态与长期工作台

> 这是本项目跨设备、跨会话的执行入口，不是完整技术文档。
> 开始工作前先阅读本文；需要实现细节时再查阅 `README.md` 中对应的功能章节与 `ARCHITECTURE.md`。

## 文档状态

- 最近更新：2026-08-12
- 当前分支：`main`
- 当前构建环境：Visual Studio 2026、v145、Windows x64
- 当前默认验证配置：`Debug | x64`
- 当前主线：P11 Terrain Chunk、LOD 与剔除
- 主线状态：待开始（先建立固定 3×3 Chunk 与连续边界验证，再接入剔除和距离 LOD）

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
8. 功能行为、工作流或实现说明变化时更新 README；新增功能章节必须位于 `## KB` 上方并沿用既有标题格式；
9. 允许调整本文结构、合并旧记录或拆分任务，只要当前目标和依赖关系更清晰。

### 三份文档的职责

| 文档 | 唯一职责 | 每次任务结束时检查 |
| --- | --- | --- |
| `Documents/PROJECT_STATUS.md` | 当前主线、后续顺序、完成记录、验收证据和技术债 | 任务状态和下一步是否准确 |
| `ARCHITECTURE.md` | 当前已经实现的模块职责、依赖、生命周期、数据流和边界 | 源码结构或跨层关系是否变化 |
| `README.md` | 功能演进、使用方式、实现笔记、验证结果和 KB | 对外行为或开发工作流是否变化 |

三份文档允许互相引用，但不要复制整段内容。发生冲突时，以源码事实为准并在同一任务内修正文档。

### 状态定义

| 状态 | 含义 |
| --- | --- |
| 待开始 | 目标和验收条件已明确，可以直接实施 |
| 进行中 | 已产生代码或资源修改，尚未完成全部验收 |
| 阻塞 | 存在明确外部依赖，并已记录解除条件 |
| 已完成 | 实现与验收均完成，已经移入完成区 |

## 当前主线

### P11：Terrain Chunk、LOD 与剔除

**依赖**：P7 地形生成和 P8 TerrainMaterial 在单块地形上稳定。

**状态**：待开始。

**目标**

- 从固定 `3×3` Chunk 验证坐标、采样和边缘连续；
- 共享网格模板，增加 Bounds、视锥剔除和距离 LOD；
- 通过 Skirt、边缘索引或 Morph 解决 LOD 接缝；
- 规模扩大后再评估 Quadtree、Geomipmapping 或 Clipmap。

**验收**

- Chunk 边缘无裂缝和法线断层；
- 视锥外 Chunk 不提交 DrawCall；
- LOD 切换不过度跳变，近景精度和远景覆盖可独立调整。

## 后续任务

任务按依赖和建议实施顺序排列。除非用户调整方向，当前主线完成后依次提升。自动化回归可以穿插建设，但同一时刻仍只保留一个功能主线。

### P12：山脉大气表现与后处理

**依赖**：Scene Depth 世界位置重建稳定；优先在 P10 后接入环境一致性。

**目标**

- 依次实现距离雾、高度雾和主光方向驱动的雾色；
- 校准 ACES Tone Mapping，再评估 Bloom 与 TAA；
- 完整 Atmospheric Scattering、Motion Blur 和 DOF 不作为首版阻塞项。

**验收**

- 远山对比度和饱和度随距离下降；
- 天空与地形交界自然，近景不会被均匀灰雾覆盖；
- 曝光和雾效不产生重复 Tone Mapping/Gamma。

### P13：固定步长水流与运行时侵蚀

**依赖**：P6 Terrain Runtime、P7 派生资源和稳定 GPU Ping-Pong 完成。

**目标**

- 建立 Rainfall → Flux → WaterDepth/Velocity → Sediment → Erosion/Deposition 流程；
- 提供 Play、Pause、Single Step、Reset 和统计；
- 与有限次 Authoring Erosion 分开调度和保存。

**验收**

- 水深、泥沙和地形状态保持非负、有限且无 NaN；
- 无降雨/蒸发边界下质量误差处于明确容差；
- 洼地能蓄水，高处水流向低处，侵蚀和沉积方向合理；
- 固定时间步下结果不依赖编辑器帧率。

### P14：简化气候与植被闭环

**依赖**：P13 稳定，P8 能接收湿度/植被材质权重。

**目标**

- 使用单层二维场实现 Temperature、Evaporation、Humidity Advection、Orographic Lift、Condensation/Rainfall；
- 把降雨反馈到水流/侵蚀，把湿度和温度反馈到植被分布；
- 首版使用常量或程序化水平风，不直接实现三维流体大气。

**验收**

- 湿度能随风输运并在地形抬升区形成更多降雨；
- 蒸发、水汽、降雨和地表水变化具有可解释统计；
- 植被响应可复现，不把单株测试实体永久写入默认场景。

### 长期候选

- Vulkan 后端实际实现，而不只是接口预埋；
- GPU Driven Rendering、遮挡剔除和间接绘制；
- 完整三维大气与体积流体；
- Linux/macOS 应用图标和打包资源；
- 统一 Asset Command、Dirty 状态和保存提示；
- 发布构建、资源打包与项目模板。

## 已完成里程碑

此处只记录足以影响后续决策的结果。完整设计、代码片段和教学说明位于 README。

### 2026-08-12：P10 IBL 环境光照

- 完成 Radiance HDR/等距柱状图导入、线性 `RGBA16F` Cubemap、完整普通 Mip Chain，以及 Model/Terrain 共用 SkyLight 数据链；
- `EnvironmentLighting` 按源 Handle、Runtime Version、派生图类型与生成参数缓存 `32×32 / 64 samples` Diffuse Irradiance 和 `64×64 / 64 samples`、7 层 GGX Specular Prefilter，正常帧不重复读回或卷积；
- 新增与具体环境无关、进程级共享的 `64×64 RG16F / 128 samples` Split-Sum BRDF LUT；PBRModel 使用 slot 8/9/10，Terrain 使用 slot 20/21/22，并按 `Prefilter × (F0 × scale + bias)` 完成环境镜面项；
- GTX 1050 / OpenGL 4.6 下 BRDF LUT 在日志相邻秒内生成一次；Diffuse 与 Specular 各生成一次，PBR Lab 6/6，默认 Terrain、Shadow 与三条 Terrain Compute Shader 均成功加载；
- 验证：编辑器增量构建成功，78 项无窗口回归全部 PASS，新增覆盖 LUT 有限/有界及 Roughness/掠射角响应；构建产物保留，提交：待提交。

### 2026-08-11：P8.1 TerrainMaterial 采样优化

- Terrain Fragment Shader 在最终高度/坡度/曲率/湿度权重确定后只保留贡献最高的两层，再执行 Triplanar Albedo/Normal/AO 采样；高性能档进一步只为主导层采样 Normal/AO，Auto Distance 在近景 Top-2 完整细节与远景主层细节之间使用 30% 宽度的 smoothstep 过渡；
- TerrainRenderer 新增独立非阻塞 GPU Timer、采样模式、距离阈值、DrawCall/绑定纹理统计；Debug Overview 可即时切换 Full-4、Top-2、Top-2 + Dominant Normal/AO 与 Auto Distance。Auto 成为默认，Full-4 与 Handle=0 的无纹理基础颜色路径继续作为质量和性能基线；
- 新增独立 TerrainSamplingBenchmarkTool：不持有 Scene 或 EditorLayer 状态，按唯一 GPU Query 样本执行 15 次预热和每档 30 次采样；无人值守入口只为默认 Terrain 分配完整 DefaultTerrain、固定相机，完成后正常退出；
- GTX 1050/OpenGL 4.6 固定场景结果：Full-4 10.681 ms，Top-2 6.026 ms（降低 43.6%），Top-2 + Dominant Normal/AO 4.226 ms（降低 60.4%）；三档固定视口截图未发现新增条带接缝，Terrain 图形/Compute Shader 均成功编译；
- 验证：Premake VS2026 重新生成成功，VS2026 Debug x64 全解决方案构建成功，64 项无窗口断言全部 PASS；提交：待提交。

### 2026-08-11：tmpTerrain 地质地貌迁移实验

- 从 `tmp/tmpTerrain/TerrainHeightInitializer_CS.hlsl` 选择性迁移 Worley 地质块、陡峭区遮罩、狭长裂谷和大尺度趋势思想；算法已重写为现有 OpenGL GLSL Compute 分支，没有引入原型的 HLSL、固定 4096² 网格或 GPU 网格初始化；
- `TerrainNoiseSettings` 新增 GeologyBlend、GeologyScale、RiftStrength 与 TrendStrength；参数进入 Terrain Preset、Scene YAML、Inspector 连续编辑事务和 Runtime Dirty/Regenerate 链路，GeologyBlend 设为 0 时跳过新增计算并恢复原始生成公式；
- 未迁移原型的浅水、泥沙、蒸发和气象耦合 Pass：原始 `TerrainData_CS.hlsl` 在单次 Dispatch 中用 Group Barrier 后跨 Workgroup 读取输出，存在数据竞争，后续必须按固定步长拆成 Flux、Water Update、Velocity、Sediment 和 Erosion 等独立 Ping-Pong Pass；
- 验证：GTX 1050/OpenGL 4.6 下 GenerateFBM、ThermalErosion、DeriveTerrainMaps 与 Terrain 图形 Shader 编译成功；两次 GPU 输出确定性 Hash 均为 `16881604791310884879`，共 30 次 Dispatch；VS2026 `Debug | x64` 全解决方案构建成功，64 项无窗口断言全部 PASS；
- 提交：待提交。

### 2026-08-11：P9 方向光阴影与 CSM

- Model 与 Terrain 共用 1～4 级方向光 CSM，包含 Practical Split、稳定正交范围、Texel Snap、可调重叠混合、`3×3 PCF`、Slope Bias 与纯运行时级联可视化；Shadow 深度资源和矩阵不序列化，只由 Directional Light 设置重建；
- Model/Terrain Bounds 在每级 Shadow Frustum 中保守剔除；Model Shadow Queue 按 Mesh 与最终 Mask 状态排序，并以独立 Shadow VAO/Instance Buffer 合批；Mask 使用最终 MaterialInstance 的纹理 Alpha 与 Cutoff 裁剪；
- 明确半透明投影策略：Opaque 与 Mask 参与方向光阴影，Blend 默认不写 Shadow Map，避免半透明表面投出错误的实心轮廓；未来若需要彩色透射或抖动阴影，作为独立能力建设；
- 建立非阻塞 `GPUTimer`、9 组 Shadow Benchmark、无人值守入口和包含 Opaque/Mask/Blend 对照、四级深度标记及远近两种构图的 Shadow Visual Validation；
- 验证：RTX 4060 完成固定 2500 实体、9 组各 30 样本性能基准；GTX 1050/OpenGL 4.6 实际窗口检查级联覆盖/过渡、Mask 镂空、Opaque 接触阴影及 Blend 无实心阴影，默认 Bias 下未见明显大面积 Acne 或 Peter Panning；VS2026 `Debug | x64` 全解决方案构建成功，立即重复同配置构建只执行增量项目检查，64 项无窗口断言全部 PASS；
- 提交：待提交。

### 2026-08-09：Renderer2D 空批次残留修复

- 修复实体添加 `SpriteRendererComponent` 后再移除时，视口仍残留白色 Quad 的问题；根因是空 Batch 把 `indexCount = 0` 传入 `DrawIndexed`，而底层将 0 解释为绘制完整预生成索引缓冲，导致上一帧 VBO 残留被再次提交；
- `Renderer2D::Flush` 现对零索引批次直接返回，不绑定纹理、不发起 Draw Call，也不增加统计；保留其他调用方使用 `DrawIndexed(0)` 绘制完整索引缓冲的既有语义；
- 验证：VS2026 `Debug | x64` 完整编辑器目标构建成功；55 项无窗口断言及最终汇总全部 PASS；默认 Alpine 场景在 Intel Iris Xe/OpenGL 4.6 下稳定运行，Texture、ShadowDepth、Terrain 与三个 Terrain Compute Shader 均成功加载；
- 提交：待提交。

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
- 提交：待提交。

### 2026-08-09：P7 山脉生成、派生图与 Authoring Erosion

- `TerrainSpecification` 新增 Custom、Alpine、Plateau、Rolling Hills、Volcanic、Eroded Valley 六种状态，预设统一设置确定性 Seed、地貌参数、HeightScale 和有限次侵蚀参数；方向、宽度和台地强度支持继续手调，任一手调自动转为 Custom；
- `GenerateFBM.comp` 增加旋转后的各向异性 Ridged FBM 与双山链组合，使 Alpine/Eroded Valley 形成连续山脉走向；Plateau、Rolling Hills 和 Volcanic 使用各自稳定地貌分支；
- 新增 `ThermalErosion.comp`，每轮只读 HeightGrid ReadTexture、只写 WriteTexture，Barrier 后 Swap；最多 128 次，仅在 Dirty、Shader 热重载或显式 Regenerate 时执行，不进入每帧生态模拟；
- 新增 `DeriveTerrainMaps.comp`，一次 Dispatch 从最终 Height 生成 RGBA16F Normal/Slope、Curvature/Flow Potential 和 Grass/Soil/Rock/Snow Material Weights；权重归一化，Terrain Shader 已使用派生法线和权重进行基础可视化；所有派生纹理仅属于 `TerrainRuntime`；
- Scene YAML 保存 Preset、三项新增地貌参数、Authoring Erosion 和两个 Compute Shader Handle；旧场景缺 Preset 时按 Custom 读取。Inspector 提供预设、Mountain、Authoring Erosion 控件、生成版本和 Dispatch 数，全部复用 P6 的单命令事务；
- 验证：`scripts\Verify-Windows.bat` 完整通过，VS2026/MSBuild 18.8.2 `Debug | x64` 全解决方案构建成功且 46 项无窗口断言全部 PASS；Intel Iris Xe/OpenGL 4.6 下三个 Compute Shader 编译成功，默认 Alpine 每次执行 30 Dispatch；相同参数两次 GPU 输出哈希均为 `4345498711584764525`，高度与派生图无 NaN/Inf、范围合法且材质权重归一化；
- README：新增“山脉生成、派生图与 Authoring Erosion”；ARCHITECTURE：同步 Terrain 三段式 Compute 数据流和 Runtime 所有权；
- 提交：待提交。

### 2026-08-09：P6 Terrain 生命周期与编辑事务收口

- `TerrainComponent` 的复制构造和复制赋值均只保留 `TerrainSpecification` 并清空 `TerrainRuntime`，关闭 Scene Copy、实体复制、快照恢复和属性命令通过赋值共享 GPU Runtime 的漏洞；
- Terrain、Directional/Point/Sky Light 与 Camera Inspector 已统一接入 `EditorValueTransaction`：拖动期间实时预览，控件释放时只记录一条 `ValueEditorCommand`；Terrain Undo/Redo 会恢复完整规格并强制延迟重建 Runtime；高度图与 SkyLight Cubemap 的离散替换同样进入命令历史；
- 删除从未接入当前 EditorLayer 的旧 `TerrainPanel`，正式地形参数只由 `TerrainComponent` Inspector 管理；EditorLayer 继续仅负责 Scene、Pass、Framebuffer 和面板编排，不持有 TerrainMesh、HeightMap 或 Terrain Shader 业务状态；
- 无窗口回归扩展到 35 项，覆盖 Terrain Transform/实体复制、Specification YAML 往返、Runtime 非持久化、Edit → Play Scene Copy 隔离、复制赋值失效以及单条 Terrain 命令 Undo/Redo；测试目标复用编辑器 CommandHistory 实现但仍不创建窗口或图形上下文；
- 验证：`scripts\Verify-Windows.bat` 完整通过，Premake VS2026 生成、MSBuild 18.8.2 `Debug | x64` 全解决方案构建和 35 项断言全部成功；完整编辑器在正确项目工作目录下稳定运行 8 秒；已知 GLFW Premake 弃用警告仍保留；
- README：新增“Terrain 生命周期与 Inspector 编辑事务收口”，并修正旧 TerrainPanel 说明；ARCHITECTURE：同步 Runtime 所有权、Inspector 事务覆盖和测试边界；
- 提交：待提交。

### 2026-08-09：P5 自动化回归测试与可重复构建验证

- 新增独立 `GlimmerRegressionTests` ConsoleApp，不创建窗口、Application 或渲染上下文；覆盖旧 `.glmat` 默认兼容、完整 Material 保存/重载、MaterialOverrides 启用字段合并与数值 Clamp；
- 最小内存 Scene 使用固定 UUID、Transform、ModelHandle 和完整 PBR MaterialOverrides 保存为临时 `.glimmer`，随后加载到新 Scene，并通过 `FindEntityByUUID` 验证稳定身份、组件和 Handle/Mask/Values 往返；测试临时目录位于系统 Temp，退出时递归清理，不写入默认编辑场景；
- 测试程序逐项输出 PASS/FAIL，任一断言失败返回 1；`--force-failure` 已验证调用链能稳定传播非零退出码；
- 新增 `scripts/Verify-Windows.bat` 无暂停入口及其 PowerShell 实现：绕过本机脚本执行策略后检查递归子模块是否初始化，调用仓库内 Premake 生成 VS2026 `.slnx`，自动查找或接收显式 MSBuild 路径，构建全解决方案 `Debug | x64`，最后运行无窗口测试；
- 验证：统一脚本完整通过；Premake 成功生成 `GlimmerRegressionTests.vcxproj`，VS2026/MSBuild 18.8.2 全解决方案构建成功，23 项正常断言全部 PASS；强制失败运行返回退出码 1；测试临时目录自动清理；
- README：新增“无窗口回归测试与 Windows 一键验证”，记录跨设备 Clone、子模块、生成、构建和测试流程；ARCHITECTURE：补充独立测试目标及其依赖/隔离边界；
- 提交：待提交。

### 2026-08-09：PBR 材质通道与颜色空间契约

- `MaterialProperties`、`.glmat` 与实体 `MaterialOverrides` 已同步加入 Normal/AO/Emissive Texture、NormalScale、AOStrength、EmissiveColor/Strength；旧文件缺字段时维持无贴图、Normal/AO 强度 1、Emissive 强度 0 的兼容默认值；
- 共享 Material Inspector 与实体 Override Inspector 均支持新字段、贴图拖放、连续编辑事务和 Undo/Redo；场景 YAML 保存完整 Mask/Values，MaterialInstance 缓存比较最终完整状态；
- Renderer3D 使用固定 0～3 纹理单元绑定 BaseColor、Normal、AO、Emissive，并把四纹理 GPU ID、存在状态和全部参数纳入排序/合批键；不透明 Instancing、Mask/Blend 和 EntityID 路径继续复用同一 PBR Shader；
- PBRModel 使用切线空间 Normal Mapping，AO 仅调制环境项，Emissive 在线性 HDR 空间累加；BaseColor/Emissive 使用 sRGB 资产，Normal/AO 使用 Linear，Renderer 只读取符合语义契约的纹理且不在绘制阶段改写注册表；模型切线生成对退化 UV 增加稳定正交基回退；
- Debug Rendering 新增独立 `PBRMaterialLabTool`，生成 6 个临时材质球对照 Normal、AO、Emissive、Dielectric/Metallic 与 Smooth/Rough；支持 `GLIMMER_PBR_LAB_AUTORUN=1` 自动生成并执行材质/场景 YAML 往返验证；
- 验证：Premake VS2026 重新生成成功；VS2026 `Debug | x64` 全解决方案构建成功；自动 Lab 稳定运行 8 秒并记录 `6/6 items` 渲染 PASS、旧 `.glmat` 与全部新 Override YAML 往返 PASS；测试进程正常关闭，临时文件/日志已清理；`git diff --check` 通过；
- 未覆盖：Metallic/Roughness 仍为标量，独立贴图或打包 ORM 通道、镜像 UV 的 Tangent Handedness、IBL 与阴影留给后续；
- README：新增“PBR 材质纹理通道扩展与 Material Lab”；
- 提交：待提交。

### 2026-08-09：可扩展 Debug 面板与 GPU Instancing Lab

- 当前完整编辑器新增独立 `Window → Debug` 面板，以 Overview/Rendering 页签承载长期诊断入口；首个独立工具 `InstancingLabTool` 不侵入 Renderer3D；
- Instancing Lab 使用临时内存 Scene 创建真实 ECS 模型实体，退出、切换场景、进入 Play 或编辑器关闭时恢复原 EditorScene；Lab 不进入 Undo/Redo，不允许保存，默认暂停 Hierarchy 全量枚举以免大量 ImGui 行干扰渲染压力测试；
- 默认 Cube/DefaultPBR 可生成 `50×1×50=2500` 个实体；支持 Maximum Instancing、双 Roughness Material Split 和 Blend Transparent Comparison，并可拖放替换 Model/Material；
- 面板根据实体数、Submesh 数、1024 实例分块和预设计算理论 Items、DrawCall、Instanced/Individual Draw 与 InstanceCount，逐帧对照 Renderer3D Statistics 显示 Pending/PASS/FAIL，并提供首/中/末代表实体拾取入口；
- 验证：重新生成 VS2026 工程后 `Debug | x64` 全解决方案构建成功；完整编辑器稳定运行 8 秒；`git diff --check` 通过；
- README：新增“可扩展 Debug 面板与 GPU Instancing Lab”；
- 提交：待提交。

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
- 提交：待提交。

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
- 提交：待提交。

### 2026-08-05：建立三文档同步制度

- 将 `PROJECT_STATUS.md`、`ARCHITECTURE.md` 和 `README.md` 确立为进度、架构事实和功能说明三个互补的信息源；
- 要求每次任务完成前同步审查三份文档，并按职责更新，避免不同设备和会话获得过期上下文；
- 在根 `AGENTS.md` 固化读取、更新和完成检查规则；
- 验证：三份文档职责与更新触发条件已交叉核对，`git diff --check`；
- 提交：待提交。

### 2026-08-05：架构文档同步

- 以当前源码重写 `ARCHITECTURE.md`，补齐构建、应用生命周期、资产、场景、编辑器与现代渲染链路；
- 明确当前完整编辑器、较早宿主、OpenGL 可运行后端与 Vulkan 接口预埋之间的边界；
- 同步记录 MaterialInstance、Edit/Play、CommandHistory 和尚未实现的 RenderQueue/材质事务；
- 验证：文档结构与关键源码逐项核对，`git diff --check`；
- 提交：待提交。

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

- 部分 Git 子模块包含 Premake 生成的未跟踪文件，可能使根仓库显示子模块为脏状态；不要在不确认内容的情况下清理或重置子模块；
- VS2026 Premake 生成的主入口是 `GlimmerEngine.slnx`；旧 `GlimmerEngine.sln` 可能来自 VS2022 或早期生成，自动验证脚本优先构建 `.slnx`；
- SPIRV-Cross 上游 Premake 会递归包含 samples/tests，根 Premake 当前通过 `removefiles` 排除；修改依赖生成逻辑时必须复验；
- GLFW Premake 仍使用已弃用的 `flags`、`NoRuntimeChecks` 和 `NoIncrementalLink` 写法，会产生生成警告。
- Model 资源已恢复 Cube、Plane、UV Sphere、bunny、planet、spacecraft、suzanne；注册表中的 `models/New Folder/Cube.obj`、`models/dragon.obj`、`models/UV Sphere.obj` 仍缺少源文件，需要从原设备恢复或移除失效条目。

### 编辑器

- Undo/Redo 已覆盖实体生命周期、组件增删重置、Transform、Material、Terrain、Light 与 Camera；Tag、SpriteRenderer、ModelRenderer 等部分属性仍有直接修改路径；
- Material Asset 已具备保存、撤销和失败反馈；TerrainMaterial 可显式保存/重载但尚未接入 Asset Command/Undo 和统一退出 Dirty 提示；其它共享 Asset 仍缺少统一保存协议；
- 通用组件值事务目前位于 InspectorPanel；后续新增连续控件应复用激活快照/释放提交边界，避免逐帧命令。

### 渲染

- Renderer3D 已有 Opaque/Mask/Transparent Queue、状态排序、Opaque Instancing 和 MaterialInstance 缓存；Transparent 首版仍按实体原点而不是 Mesh Bounds 中心排序，且不支持透明实例化或 OIT；
- AlphaMode Shader 契约当前由 PBRModel 完整实现；自定义 3D Shader 若要正确支持 Mask/Blend，仍需自行声明并使用 `u_AlphaMode`、`u_AlphaCutoff`；
- PBRModel 已支持 Normal、AO 与 Emissive Texture；Metallic/Roughness 仍为标量，尚未定义独立贴图或 ORM 打包通道；当前 Vertex Tangent 不包含镜像 UV 所需的 Handedness；
- Renderer2D 仍固定使用 TextureShader，`.glmat` 的 ShaderHandle 尚未参与批次兼容判断；
- 完整编辑器的 Sprite 统一在 Skybox 后、3D Transparent 前 Flush；Renderer2D 尚无独立 AlphaMode、透明距离排序或与 3D Transparent 的跨队列排序，零 Alpha 的 EntityID/深度语义仍需后续单独收口；
- Terrain 已生成 Height、Normal/Slope、Curvature/Flow Potential 与 Material Weights，接入四层 Triplanar PBR 并参与 1～4 级方向光 CSM；仍缺少 Chunk/LOD 和固定步长 Runtime Erosion 调度；
- CSM 已完成 Practical Split、Texel Snap、可调重叠混合、基于 Bounds 的 Shadow Frustum 剔除、运行时级联着色、Alpha Mask 投影和每级 Model Instancing；Terrain 仍独立提交，Blend 默认不参与 Shadow Pass，尚无彩色透射或抖动式半透明阴影；
- SkyLight 已支持六面 LDR/等距柱状 HDR、线性 `RGBA16F`、完整普通 Mip Chain、内存派生缓存，以及 Model/Terrain 共用的 Diffuse Irradiance、GGX Specular Prefilter 和 Split-Sum BRDF LUT；尚无持久化磁盘缓存、环境旋转、局部 Reflection Probe 或动态场景反射；
- 环境模拟尚未定义固定步长调度器、Simulation Asset/Component 边界和质量守恒统计；
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
