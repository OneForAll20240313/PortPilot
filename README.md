# PortPilot

PortPilot 串口调试与终端工作台（Qt/C++，DDD 五层架构）。

> 本文档为代码仓库导航。项目级强制准则（需求 / 验收 / 研发规范 / 仓库治理 / 沙箱机制 / 开发环境 / 编译约束）统一沉淀在**文档仓库** `OneForAll20240313/portpilot-design-docs` 的根级《项目共创准则.md（AGENTS.md）》，所有共创智能体必须遵守。

## 技术栈

- C++17 / CMake（仅构建目录 `build/` 编译，禁止在源码目录编译）
- Qt5 / Qt6（`QSerialPort` 串口、`QSettings` 持久化）
- GoogleTest + GMock（单元测试框架）

## 目录结构（五层架构）

| 目录 | 层 | 职责 | 依赖 |
|------|----|------|------|
| `src/core/` | 核心层 | RingBuffer / AppendBuffer / DoubleBuffer | 无（纯 C++，可独立测试） |
| `src/domain/` | 领域层 | Session / Protocol | core |
| `src/service/` | 服务层 | EventBus / ViewManager / SerialWorker | core, domain |
| `src/ui/` | UI 适配层 | Qt 界面（串口 / 终端 / 可视化） | core, domain, service |
| `tests/` | 测试 | 单元测试（GoogleTest） | 各层 |

依赖方向单向：`core <- domain <- service <- ui`，禁止反向依赖。

> 契约来源：`产品/portpilot-design/contracts/`（`buffer.schema.json`、`session.schema.json`、`protocol.schema.json`、`service-api.md` 等），实现不得偏离契约。

## 构建与测试

```bash
# 配置（独立构建目录，禁止源码目录编译）
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug
# 构建
cmake --build build -j
# 运行测试
ctest --test-dir build --output-on-failure
```

## CI

- `.github/workflows/ci.yml`：push/PR 触发，构建 + 运行单元测试，经 `dorny/test-reporter` 发布到 PR Checks，产物归档可追溯。
- 提交推送后必须等待 CI 通过方可继续后续任务（见准则「研发规范 · CI 通过门槛」）。

## 状态

- 工程骨架已就绪，UI 层待实现（当前为占位入口）。
- 需求 / 验收 / 设计权威版本见文档仓库。