# Glimmer 项目状态与长期工作台

> 这是本项目跨设备、跨会话的执行入口，不是完整技术文档。
> 开始工作前先阅读本文；需要实现细节时再查阅 `README.md` 中对应的功能章节与 `ARCHITECTURE.md`。

## 文档状态

- 最近更新：2026-08-09
- 当前分支：`main`
- 当前构建环境：Visual Studio 2026、v145、Windows x64
- 当前默认验证配置：`Debug | x64`
- 当前主线：PBR 材质通道扩展
- 主线状态：待开始

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

### PBR 材质通道扩展

**目的**

在已稳定的 Material、Opaque/Transparent RenderQueue、AlphaMode 和基础 Cook–Torrance PBR 之上，补齐标准模型材质所需的纹理通道与颜色空间契约。

**当前问题**

- 当前只有 BaseColor、Metallic、Roughness，缺少 Normal、AO、Emissive 通道；
- 颜色纹理和数据纹理需要明确区分 sRGB 与 Linear，避免重复 Gamma；
- `.glmat`、MaterialOverrides、Inspector、缓存排序键和 Shader 必须保持同一字段契约；
- 旧材质和旧场景必须继续按现有默认值加载。

**实施范围**

- [ ] 增加 Normal、AO、Emissive 纹理和必要强度参数；
- [ ] 同步 `.glmat`、MaterialOverrides、Inspector、Undo/Redo、YAML 与 MaterialInstance 缓存；
- [ ] 明确 BaseColor/Emissive 使用 sRGB，Normal/AO/Metallic/Roughness 使用 Linear；
- [ ] 扩展 PBRModel Shader，并保持 Opaque、Mask、Blend 和 EntityID 语义；
- [ ] 保持旧材质/场景兼容以及现有 Opaque Instancing 合批正确性；
- [ ] 更新 README 和 ARCHITECTURE 对应章节。

**暂不包含**

- 阴影与 CSM；
- IBL 环境卷积；
- Height/Parallax Mapping；
- 遮挡剔除；
- GPU Driven Rendering；
- Vulkan 后端。

**验收条件**

- [ ] 标准材质球可分别验证 Normal、AO、Emissive 和 Metallic-Roughness；
- [ ] sRGB 颜色纹理与 Linear 数据纹理不发生重复 Gamma；
- [ ] 旧 `.glmat` 和场景文件仍可加载；
- [ ] Material Asset、MaterialOverrides、Inspector 与 YAML 往返一致；
- [ ] Opaque/Mask/Blend、Instancing、EntityID 和统计不退化；
- [ ] VS2026 `Debug | x64` 全量构建成功；
- [ ] 编辑器启动并稳定渲染首帧；
- [ ] `git diff --check` 通过。

## 后续任务

任务按依赖和建议实施顺序排列。除非用户调整方向，当前主线完成后依次提升。自动化回归可以穿插建设，但同一时刻仍只保留一个功能主线。

### P5：自动化回归测试与持续构建

**依赖**：可以与任一功能阶段穿插，不依赖渲染架构全部完成。

**目标**

- 增加 Material 序列化、Override 合并、UUID 恢复等无窗口测试；
- 增加 Premake 生成和 VS2026 Debug 构建检查；
- 建立最小场景加载/保存往返测试；
- 记录不同设备拉取子模块、生成工程和构建的标准流程。

**验收**

- 无窗口测试能稳定复现失败并返回非零退出码；
- Premake、VS2026 `Debug | x64` 和最小场景往返有可重复执行入口；
- 每个后续 Lab 场景独立运行，不向默认编辑场景永久写入测试实体。

### P6：Terrain 现状复核与编辑事务收口

**依赖**：当前主线完成；可与 P4 并行准备，但不得重建已经存在的 TerrainComponent/TerrainRenderer。

**目标**

- 复核 Terrain Transform、Scene Copy、YAML 往返和 Runtime Invalidate；
- 将 Terrain、Light、Camera 等剩余连续属性接入统一 Undo/Redo 事务；
- 清除残余 EditorLayer 地形所有权和过时测试路径；
- 正式参数保留在组件 Inspector，独立 Panel 只承担诊断。

**验收**

- Terrain Entity 移动、复制、保存/加载、Edit → Play → Stop 结果一致；
- 连续参数拖动只生成一条可逆命令；
- EditorLayer 不持有 TerrainMesh、HeightMap 或 Terrain Shader 的业务状态。

### P7：山脉生成、派生图与 Authoring Erosion

**依赖**：P6 完成，SimulationGrid、Compute 与 Terrain Runtime 边界稳定。

**目标**

- 形成连续山脉走向而非均匀随机尖峰；
- 加入有限次 Thermal/Authoring Erosion；
- 生成 Height、Normal、Slope、Curvature、Flow 与 Material Weights；
- 提供 Alpine、Plateau、Rolling Hills、Volcanic、Eroded Valley 等稳定预设。

**验收**

- 同一 Seed 和参数生成结果确定；
- 只在 Dirty 或显式 Regenerate 时 Dispatch；
- 所有 Compute Pass 无 NaN，且不存在无保护的同纹理读写；
- Authoring Erosion 不进入每帧生态模拟循环。

### P8：TerrainMaterial 与分层 PBR

**依赖**：P4、P7 完成。

**目标**

- 明确 TerrainMaterial 的资产类型、注册表元数据、序列化和 Inspector 入口；
- 建立 Grass、Soil、Rock、Snow 四层材质；
- 按高度、坡度、曲率、湿度和权重混合；
- 使用 Triplanar Mapping 避免陡坡 UV 拉伸。

**验收**

- 陡坡以岩石为主，高处平缓区域可积雪，湿润低地可驱动土壤/植被层；
- 山壁无明显 UV 拉伸，材质计算保持 HDR 线性空间；
- TerrainMaterial 可保存、重载、拖放且不污染普通 `.glmat` 布局。

### P9：方向光阴影与 CSM

**依赖**：P4 的直接光材质验证稳定；P8 可先使用单级阴影。

**目标**

- 先实现 Directional Shadow Map，再扩展 3～4 级 CSM；
- 建立独立 Shadow Pass、深度资源和偏移策略；
- 让 Model 与 Terrain 共用一致的阴影约定。

**验收**

- 山峰和模型遮挡关系稳定，无明显 Peter Panning 或大面积 Acne；
- 级联切换不过度跳变，视锥外阴影对象不提交；
- Shadow 资源不写入场景文件，只由可序列化设置重建。

### P10：IBL 环境光照

**依赖**：P4 的 PBR、颜色空间和 TextureCube 方向约定稳定。

**目标**

- 支持 HDR 经纬图导入、浮点 Cubemap 和完整 Mip Chain；
- 依次实现 Diffuse Irradiance、Specular Prefilter 和 BRDF LUT；
- 以源环境 Handle、资源版本和生成参数缓存派生贴图；
- Terrain 与 Model 使用同一 SkyLight 环境数据。

**验收**

- 关闭方向光后材质仍有合理环境照明；
- Roughness 增大时环境反射逐渐模糊；
- Cubemap 无翻转、明显接缝或重复 Gamma；
- 同一环境资源不会逐帧卷积或重复生成缓存。

### P11：Terrain Chunk、LOD 与剔除

**依赖**：P7 地形生成和 P8 TerrainMaterial 在单块地形上稳定。

**目标**

- 从固定 `3×3` Chunk 验证坐标、采样和边缘连续；
- 共享网格模板，增加 Bounds、视锥剔除和距离 LOD；
- 通过 Skirt、边缘索引或 Morph 解决 LOD 接缝；
- 规模扩大后再评估 Quadtree、Geomipmapping 或 Clipmap。

**验收**

- Chunk 边缘无裂缝和法线断层；
- 视锥外 Chunk 不提交 DrawCall；
- LOD 切换不过度跳变，近景精度和远景覆盖可独立调整。

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
- SPIRV-Cross 上游 Premake 会递归包含 samples/tests，根 Premake 当前通过 `removefiles` 排除；修改依赖生成逻辑时必须复验；
- GLFW Premake 仍使用已弃用的 `flags`、`NoRuntimeChecks` 和 `NoIncrementalLink` 写法，会产生生成警告。
- Model 资源已恢复 Cube、Plane、UV Sphere、bunny、planet、spacecraft、suzanne；注册表中的 `models/New Folder/Cube.obj`、`models/dragon.obj`、`models/UV Sphere.obj` 仍缺少源文件，需要从原设备恢复或移除失效条目。

### 编辑器

- Undo/Redo 尚未覆盖所有组件属性；
- Material Asset 已具备保存、撤销和失败反馈；其它共享 Asset 仍缺少统一 Dirty、保存和退出提示；
- Terrain、Light、Camera 等属性仍有直接修改路径；
- 已有通用值事务辅助层，但 Terrain、Light、Camera 等连续控件尚未迁移。

### 渲染

- Renderer3D 已有 Opaque/Mask/Transparent Queue、状态排序、Opaque Instancing 和 MaterialInstance 缓存；Transparent 首版仍按实体原点而不是 Mesh Bounds 中心排序，且不支持透明实例化或 OIT；
- AlphaMode Shader 契约当前由 PBRModel 完整实现；自定义 3D Shader 若要正确支持 Mask/Blend，仍需自行声明并使用 `u_AlphaMode`、`u_AlphaCutoff`；
- Renderer2D 仍固定使用 TextureShader，`.glmat` 的 ShaderHandle 尚未参与批次兼容判断；
- 完整编辑器的 Sprite 统一在 Skybox 后、3D Transparent 前 Flush；Renderer2D 尚无独立 AlphaMode、透明距离排序或与 3D Transparent 的跨队列排序，零 Alpha 的 EntityID/深度语义仍需后续单独收口；
- Terrain 仍缺少正式 TerrainMaterial 资产、派生图缓存、Chunk/LOD 和运行时侵蚀调度；
- SkyLight 目前只绘制可见 Cubemap，尚无 HDR 环境导入、Diffuse/Specular IBL 和派生缓存；
- 环境模拟尚未定义固定步长调度器、Simulation Asset/Component 边界和质量守恒统计；
- Vulkan 目前只有接口和依赖预埋，没有可运行后端。

## 固定验证清单

根据任务范围选择必要项；触及核心构建、场景、资产或渲染时应执行完整清单。

- [ ] `git diff --check`
- [ ] 运行 `scripts\Win-GenerateProject-vs2026.bat`（Premake 或文件列表变化时）
- [ ] VS2026 `Debug | x64` 全解决方案构建
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
