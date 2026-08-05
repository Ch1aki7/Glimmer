# Glimmer 项目状态与长期工作台

> 这是本项目跨设备、跨会话的执行入口，不是完整技术文档。
> 开始工作前先阅读本文；需要实现细节时再查阅 `README.md` 中对应的功能章节与 `ARCHITECTURE.md`。

## 文档状态

- 最近更新：2026-08-05
- 当前分支：`main`
- 当前构建环境：Visual Studio 2026、v145、Windows x64
- 当前默认验证配置：`Debug | x64`
- 当前主线：3D RenderQueue 与状态排序
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

### 3D RenderQueue 与状态排序

**目的**

将 `Renderer3D::DrawModel()` 的立即绘制拆为 Submit 与 Execute，建立稳定的不透明渲染队列和状态排序基础，为后续 Instancing 与 MaterialInstance 缓存提供批次边界。

**当前问题**

- Renderer3D 逐实体解析 Model、Material、Shader 并立即逐 Mesh 绘制；
- 相邻物体即使共享 Shader、Material 或纹理，也会重复绑定状态；
- 缺少可测量的 DrawCall、ShaderBind、TextureBind 等 3D 统计；
- 尚无稳定的 RenderItem/RenderKey，可见性、透明队列和 Instancing 无统一提交边界。

**实施范围**

- [ ] 将 3D 绘制拆分为场景 Begin、Submit、Execute/End；
- [ ] 建立包含 Mesh、Shader、Material 状态、Transform 和 EntityID 的 `RenderItem`；
- [ ] 建立稳定的 Opaque `RenderKey`，按 Shader、Material、Texture、Mesh 排序；
- [ ] 保持 EntityID 输出，确保编辑器鼠标拾取不退化；
- [ ] 缓存当前绑定状态，避免无意义的 Shader 和 Texture 重绑；
- [ ] 增加 DrawCall、SubmittedItem、ShaderBind、TextureBind 等统计；
- [ ] 保留不兼容或无效资源的安全跳过行为；
- [ ] 更新 README 和 ARCHITECTURE 对应章节。

**暂不包含**

- 透明物体队列和距离排序；
- 3D Instancing 与 Instance Buffer；
- 遮挡剔除；
- Vulkan 后端。

**验收条件**

- [ ] 默认场景与改造前渲染结果一致；
- [ ] 改变实体提交顺序不影响不透明物体结果；
- [ ] EntityID 拾取保持正确；
- [ ] 统计能够证明共享状态场景中的绑定次数下降；
- [ ] 无效 Model、Material 或 Shader 不导致崩溃；
- [ ] VS2026 `Debug | x64` 全量构建成功；
- [ ] 编辑器启动并稳定渲染首帧；
- [ ] `git diff --check` 通过。

## 后续任务

任务按依赖和建议实施顺序排列。除非用户调整方向，当前主线完成后依次提升。

### P2：3D Instancing 与 MaterialInstance 缓存

**依赖**：RenderQueue 与 RenderKey 稳定。

**目标**

- 为相同 Mesh、Shader、Material 和兼容渲染状态建立实例批次；
- 使用 Instance Buffer 上传 Transform 与 EntityID；
- 为 MaterialInstance 增加 Dirty/version 或解析结果缓存；
- 不兼容 Instancing 的 Shader 自动回退普通 Draw；
- 添加实例数、批次数和节省 DrawCall 的统计。

### P3：PBR 材质通道扩展

**依赖**：材质事务和 RenderQueue 完成。

**目标**

- 扩展 Normal、AO、Emissive 等纹理与参数；
- 同步扩展 `.glmat`、MaterialOverrides、Inspector 和序列化；
- 明确线性空间与 sRGB 纹理导入规则；
- 保持旧材质文件向后兼容。

### P4：自动化回归测试与持续构建

**依赖**：可以与 P1/P2 穿插，不依赖渲染架构完成。

**目标**

- 增加 Material 序列化、Override 合并、UUID 恢复等无窗口测试；
- 增加 Premake 生成和 VS2026 Debug 构建检查；
- 建立最小场景加载/保存往返测试；
- 记录不同设备拉取子模块、生成工程和构建的标准流程。

### 长期候选

- Vulkan 后端实际实现，而不只是接口预埋；
- Linux/macOS 应用图标和打包资源；
- 透明物体 RenderQueue 与深度排序；
- 阴影、IBL 和更完整的 PBR 管线；
- 统一 Asset Command、Dirty 状态和保存提示；
- 发布构建、资源打包与项目模板。

## 已完成里程碑

此处只记录足以影响后续决策的结果。完整设计、代码片段和教学说明位于 README。

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

### 编辑器

- Undo/Redo 尚未覆盖所有组件属性；
- Material Asset 已具备保存、撤销和失败反馈；其它共享 Asset 仍缺少统一 Dirty、保存和退出提示；
- Terrain、Light、Camera 等属性仍有直接修改路径；
- 已有通用值事务辅助层，但 Terrain、Light、Camera 等连续控件尚未迁移。

### 渲染

- Renderer3D 仍逐模型、逐 Mesh 立即绘制；
- 没有 RenderQueue、RenderKey、状态排序或 3D Instancing；
- MaterialInstance 当前按提交临时解析，没有 Dirty/version 缓存；
- Renderer2D 仍固定使用 TextureShader，`.glmat` 的 ShaderHandle 尚未参与批次兼容判断；
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
