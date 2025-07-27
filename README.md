# 粤嵌 GEC6818：智能车库管理系统 
2025年暑假西南财经大学天府学院实训项目
# 无人车库进出管理系统
![语言](https://img.shields.io/badge/language-C%2FC%2B%2B-blue.svg)
![框架](https://img.shields.io/badge/framework-OpenCV%20%7C%20Caffe-orange.svg)
![数据库](https://img.shields.io/badge/database-SQLite3-green.svg)
![平台](https://img.shields.io/badge/platform-GEC6818-lightgrey.svg)

> 一个基于粤嵌GEC6818嵌入式ARM Cortex-A53硬件平台搭载嵌入式Linux系统，实现停车系统的组建，实现低功耗，低成本，高效率的智能停车管理平台。
## 目录 (Table of Contents)

- [1. 项目概述 (Project Overview)](#1-项目概述-project-overview)
- [2. 核心功能 (Core Features)](#2-核心功能-core-features)
- [3. 系统架构 (System Architecture)](#3-系统架构-system-architecture)
  - [硬件组件 (Hardware Components)](#硬件组件-hardware-components)
  - [软件模块 (Software Modules)](#软件模块-software-modules)
- [4. 技术栈与依赖 (Technology Stack & Dependencies)](#4-技术栈与依赖-technology-stack--dependencies)
- [5. 标准工作流程 (Standard Operating Procedure)](#5-标准工作流程-standard-operating-procedure)
- [6. 项目文件结构 (Project File Structure)](#6-项目文件结构-project-file-structure)
- [7. 深度学习模型规格 (Deep Learning Model Specifications)](#7-深度学习模型规格-deep-learning-model-specifications)
- [8. 编译与执行指南 (Build & Execution Guide)](#8-编译与执行指南-build--execution-guide)
  - [编译 (Compilation)](#编译-compilation)
  - [执行 (Execution)](#执行-execution)
- [9. 环境与平台注意事项 (Environmental & Platform Notes)](#9-环境与平台注意事项-environmental--platform-notes)
- [10. 许可证声明 (License Declaration)](#10-许可证声明-license-declaration)

## 1. 项目概述 (Project Overview)

本文旨在阐述一个基于粤嵌GEC6818嵌入式Linux平台构建的智能车库管理系统。该系统通过集成自动车牌识别（ALPR） 与射频识别（RFID） 技术，实现了对车辆出入流程的高度自动化与无人化管控。在硬件层面，系统整合了摄像头、液晶触摸屏及RFID读取器等模块；在软件层面，则运用了OpenCV计算机视觉库与Caffe深度学习框架。此设计不仅确保了车辆身份识别的精确性，亦提供了完善的图形用户界面、实时语音反馈及数据管理功能，从而构成一套完整、可靠且智能化的车辆管理解决方案。

## 2. 核心功能 (Core Features)

- 🚗 **双模智能识别**: 支持基于深度学习的车牌识别和RFID刷卡两种方式，灵活应对不同场景。
- 📊 **自动化流程管理**: 自动完成车辆入库信息记录、出库计费结算的全过程，无需人工干预。
- 🖥️ **可视化交互界面**: 通过LCD触摸屏提供直观的图形用户界面（GUI），方便用户操作与信息查看。
- 🔊 **实时语音播报**: 在车辆进出、识别成功/失败等关键节点提供清晰的语音提示，提升用户体验。
- 💾 **可靠数据存储**: 使用 SQLite 轻量级数据库，持久化存储车辆信息、进出场记录及费用详情。
- ⚙️ **模块化软件设计**: 系统采用多线程架构，将各功能模块解耦，确保系统运行的稳定性和可扩展性。

## 3. 系统架构 (System Architecture)

系统整体架构分为硬件层和软件层，两者协同工作，实现完整的业务逻辑。

### 硬件组件 (Hardware Components)

| 组件 | 功能描述 |
| :--- | :--- |
| **GEC6818 开发板** | 作为系统的核心处理单元，搭载嵌入式 Linux 操作系统，负责整体的运算与控制。 |
| **摄像头模块** | 用于实时捕获车辆图像，为自动车牌识别算法提供原始数据输入。 |
| **LCD 显示屏** | 作为主要信息输出设备，用于呈现图形用户界面、车辆数据及系统运行状态。 |
| **触摸屏** | 作为主要的人机交互接口，用于接收操作员的指令输入。 |
| **RFID 读卡器** | 用于读取 RFID 标签信息，作为车牌识别之外的辅助或替代身份验证方案。 |
| **串口通信模块** | 提供标准的串行通信接口，主要用于系统调试及与外部设备的通信。 |

### 软件模块 (Software Modules)

| 模块 | 功能描述 |
| :--- | :--- |
| **主控模块 (`main.c`)** | 作为应用程序的入口点，负责系统级初始化、线程创建与管理以及资源的统一调度。 |
| **车牌识别模块 (`main.cpp`)**| 系统的核心识别引擎，该模块集成了 OpenCV 与 Caffe 框架，专职执行车牌定位、分割与字符识别任务。|
| **摄像头模块 (`camera.c`)** | 负责驱动摄像头硬件，并执行图像数据的采集、格式转换与预处理等操作。 |
| **显示模块 (`lcd.c`)** | 负责 LCD 屏幕的底层驱动与上层 UI 的绘制、刷新及管理。 |
| **触摸屏模块 (`touch.c`)** | 负责监听、解析并上报用户的触摸事件，以驱动界面交互逻辑。 |
| **RFID 模块 (`rfid.c`)** | 负责与 RFID 读卡器硬件通信，执行卡片侦测、数据读取与解析。 |
| **数据库模块 (`mySQLite.c`)**| 对 SQLite3 的 API 进行封装，为上层应用提供标准化的数据库接口（CRUD 操作）。 |

## 4. 技术栈与依赖 (Technology Stack & Dependencies)

- **核心开发语言**: C/C++
- **核心框架**: OpenCV 3.4, Caffe
- **数据库系统**: SQLite3
- **并发技术**: Pthread
- **图像编解码库**: libjpeg

## 5. 标准工作流程 (Standard Operating Procedure)

1.  **系统初始化 (System Initialization)**: 系统启动后，将完成所有硬件模块的初始化，并加载预训练的深度学习模型。
2.  **操作模式选择 (Operational Mode Selection)**: 主界面呈现“车辆入库”与“车辆出库”两个操作选项，系统在此阶段等待并响应用户的触控指令。
3.  **入场流程 (Entry Procedure)**:
    - 系统根据预设模式，激活摄像头或RFID感应器以进行车辆识别。
    - 通过车牌识别算法或读取RFID标签，采集车辆的唯一身份标识。
    - 将该车辆标识符及当前系统时间戳记录至数据库。
    - 操作结果（如“入场成功”）将通过LCD屏幕显示并辅以语音播报进行确认。
4.  **出场流程 (Exit Procedure)**:
    - 系统再次执行车辆身份识别流程。
    - 利用获取的车辆标识符，在数据库中检索其对应的入场记录。
    - 系统根据时间戳自动计算停车时长并生成相应费用。
    - 更新数据库中的车辆状态，并记录费用详情。
    - 在LCD屏幕上显示出场信息与计费结果，同时进行语音播报。
    - 操作结果（如“出场成功”）将通过LCD屏幕显示并辅以语音播报进行确认。
5.  **系统退出 (System Exit)**: 系统将释放所有已分配的资源，并关闭摄像头、LCD屏幕等硬件设备。

## 6. 项目文件结构 (Project File Structure)

```
.
├── alpr/               # 自动车牌识别 (ALPR) 核心代码
├── audio/              # 系统语音提示资源 (.wav)
│   ├── car_in_*.wav    # 车辆入场提示音
│   └── car_out_*.wav   # 车辆出场提示音
├── Car/                # 系统主程序源码
├── fonts/              # UI界面字体资源 (.bmp)
├── model/              # Caffe 深度学习模型文件
│   ├── *.caffemodel    # 模型权重
│   └── *.prototxt      # 模型网络结构
├── splash/             # 系统启动画面资源 (.bmp)
└── ui/                 # 用户界面布局与图片资源
```

## 7. 深度学习模型规格 (Deep Learning Model Specifications)

本系统的车牌识别功能依赖于以下三个预训练的Caffe模型：

1.  **`HorizonalFinemapping`**: 用于车牌区域的水平方向精确定位。
2.  **`Segmentation`**: 负责将已定位的车牌图像精确地分割为单个字符区域。
3.  **`CharacterRecognization`**: 对分割后的独立字符图像进行分类与识别。

**重要提示**: 上述模型文件为本系统识别功能的核心资产，其在`model/`目录下的完整性与原始性是保障系统正常运行的先决条件。任何未经授权的移动或修改均可能导致系统功能异常。

## 8. 编译与执行指南 (Build & Execution Guide)

### 编译 (Compilation)

本项目采用 `Makefile` 进行编译管理。请在项目根目录下执行以下指令以生成可执行文件：

```bash
make
```

编译成功后，目标可执行文件 `Car` 将存放于 `bin/` 目录下。

### 执行 (Execution)

将编译产物及相关资源部署至目标硬件平台后，通过以下指令启动本系统：

```bash
./bin/Car
```

## 9. 环境与平台注意事项 (Environmental & Platform Notes)

1.  **运行环境依赖 (Runtime Environment Dependencies)**: 系统的正常运行要求目标嵌入式Linux环境中已部署所有必要的动态链接库（`.so`文件），包括但不限于`libpthread`, `libsqlite3`, `libjpeg`, `libopencv_core`等。
2.  **硬件平台适配性 (Hardware Platform Adaptability)**: 本系统的设计与实现高度耦合于`GEC6818`硬件平台。若需移植至其他平台，可能涉及对硬件驱动层代码的相应修改与适配。
3.  **数据库初始化 (Database Initialization)**: 系统在首次运行时，会自动在当前工作目录下创建并初始化所需的数据库文件（`.db`）。

## 10. 许可证声明 (License Declaration)

本项目所含之源代码及相关模型资源，其使用权限仅限于学术研究与技术交流目的。未经项目所有者书面授权，严禁用于任何形式的商业活动。
