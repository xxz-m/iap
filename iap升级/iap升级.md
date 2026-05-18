---
tags:
  - stm32
  - stm32f407
  - iap
  - bootloader
  - 嵌入式升级
topic: STM32F407 IAP 升级
status: active
created: 2026-05-05
updated: 2026-05-05
---

# STM32F407 IAP 升级学习笔记

## 1. 第一章：IAP 基础概念总览

本章目标是先把 IAP 升级的整体概念理顺。IAP 本身并不神秘，它的核心就是：先让 STM32 运行一个稳定的 Bootloader，再由 Bootloader 接收、校验、写入新的 App 程序，最后跳转到 App 运行。

一句话记忆：

```text
IAP 的本质 = Bootloader 接收新 App，把它写进 Flash，再跳过去运行。
```

### 1.1 ICP、ISP、IAP 的区别

STM32 程序烧录和升级常见有三种方式：

| 名称 | 全称 | 核心含义 | 常见方式 | 是否需要自己写 Bootloader |
|---|---|---|---|---|
| ICP | In-Circuit Programming | 外部工具给芯片烧程序 | ST-Link、J-Link、SWD、JTAG | 不需要 |
| ISP | In-System Programming | 通过芯片厂家内置 Bootloader 烧程序 | USART、USB DFU、CAN 等 | 不需要 |
| IAP | In-Application Programming | 设备运行中自己完成升级 | 串口、CAN、网口、4G、ESP32 等 | 通常需要 |

最简单的理解：

```text
ICP：用烧录器从外面烧程序。
ISP：进入厂家内置 Bootloader 后烧程序。
IAP：自己写 Bootloader，让设备自己升级。
```

注意：ISP 不等于只能用串口。它本质上是使用 STM32 系统存储器中的官方 Bootloader，只是串口是最常见的一种入口。

### 1.2 BOOT0 和 BOOT1 的作用

`BOOT0` 和 `BOOT1` 用来决定 STM32 上电复位后从哪里启动。

| BOOT0 | BOOT1 | 启动区域 | 常见用途 |
|---|---|---|---|
| 0 | x | 主 Flash | 正常运行自己的 Bootloader 或 App |
| 1 | 0 | 系统存储器 | 进入 ST 内置 Bootloader，做 ISP |
| 1 | 1 | SRAM | 从 RAM 启动，普通项目很少用 |

`x` 表示 BOOT1 是 0 或 1 都不影响。

IAP 项目最常见配置：

```text
BOOT0 = 0
BOOT1 = 0
```

这样 STM32 上电后会从主 Flash 的 `0x08000000` 启动，也就是先运行我们自己写的 Bootloader。

常见硬件接法：

```text
BOOT0：默认 10K 下拉到 GND，可选用按键或跳帽拉高进入 ISP
BOOT1/PB2：通常 10K 下拉到 GND
```

### 1.3 Bootloader 是什么

Bootloader 是 STM32 上电后最先运行的一小段程序，它负责启动判断和升级管理。

它主要做：

- 判断是否需要升级。
- 接收新固件。
- 擦除 App 区。
- 写入新 App。
- 校验新 App。
- 跳转到 App。

它不应该做：

- 复杂业务逻辑。
- 传感器算法。
- UI 主流程。
- 电机控制主流程。

一句话记忆：

```text
Bootloader 是启动管理员和升级管理员，App 才是真正干活的业务程序。
```

IAP 最重要的是 Bootloader 设计。因为 App 是会被替换的，Bootloader 是负责替换 App 的。只要 Bootloader 还在，App 损坏后仍然可以重新升级。

### 1.4 IAP 的基本运行流程

如果从空白芯片开始，完整流程分成两段。

首次烧录阶段：

```text
1. 编写 Bootloader 工程
2. 通过 ICP，也就是 ST-Link/J-Link，把 Bootloader 烧录到 0x08000000
3. 编写 App 工程
4. 修改 App 链接地址，例如 0x08010000 或 0x08020000
5. 生成 app.bin
```

后续 IAP 升级阶段：

```text
1. STM32 上电，从 0x08000000 启动 Bootloader
2. Bootloader 判断是否需要升级
3. 上位机发送 app.bin
4. Bootloader 接收 app.bin
5. Bootloader 擦除 App 区
6. Bootloader 写入新的 App
7. Bootloader 校验 App
8. Bootloader 跳转到 App
```

所以图片或教程里说的“第一步运行 Bootloader”，通常默认 Bootloader 已经提前通过 ICP 烧进芯片了。

### 1.5 Flash 分区和地址概念

STM32F407 的主 Flash 常见起始地址：

```text
0x08000000
```

如果 Bootloader 预留 64KB：

```text
Bootloader: 0x08000000 - 0x0800FFFF
App:        0x08010000 开始
```

更严谨的写法是：

```text
Bootloader: [0x08000000, 0x08010000)
```

意思是包含 `0x08000000`，不包含 `0x08010000`。`0x08010000` 已经是 App 的第一个地址。

如果 Bootloader 预留 128KB：

```text
Bootloader: 0x08000000 - 0x0801FFFF
App:        0x08020000 开始
```

IAP 里必须特别注意地址边界，因为一旦擦除地址算错，可能会把 Bootloader 自己擦掉。

### 1.6 STM32 Flash 的构成

STM32F407 内部 Flash 不只包含主程序区，还包括系统存储器、OTP、选项字节和 Flash 控制寄存器。

| 区域 | 典型地址 | 作用 |
|---|---|---|
| 主存储器 | `0x08000000` 开始 | 存放用户 Bootloader、App、常量、向量表 |
| 系统存储器 | `0x1FFF0000` 附近 | 存放 ST 出厂内置 Bootloader，用于 ISP |
| OTP 区 | `0x1FFF7800` 附近 | 一次性可编程区域，常放出厂参数 |
| 选项字节 | `0x1FFFC000` 附近 | 配置读保护、写保护、启动选项等 |
| Flash 接口寄存器 | `0x40023C00` 附近 | 控制 Flash 解锁、擦除、写入、状态标志 |

其中 IAP 最常操作的是主存储器。系统存储器主要用于 ISP 和救砖。

Flash 写入规则：

```text
擦除：把一整个扇区变成 0xFF
写入：只能把 1 写成 0
```

所以写 Flash 前必须先擦除，擦除单位通常是扇区，写入单位则取决于芯片和库函数配置。

### 1.7 向量表是什么

向量表是 STM32 的入口地址表。它告诉 CPU：

```text
上电后栈顶在哪里
复位后从哪个函数开始执行
中断发生后跳到哪个中断函数
```

向量表最前面两个 32 位数据最重要：

```text
App 起始地址 + 0：初始栈顶地址 MSP
App 起始地址 + 4：复位入口 Reset_Handler
```

例如 App 放在 `0x08010000`：

```text
0x08010000：App 初始栈顶 MSP
0x08010004：App Reset_Handler
0x08010008：App NMI_Handler
0x0801000C：App HardFault_Handler
```

IAP 里不是手动把每个中断函数往后移，而是让整个 App 工程链接到新的 Flash 起始地址，并在运行时设置：

```c
SCB->VTOR = 0x08010000;
```

一句话记忆：

```text
App 放到哪里，App 的向量表就要重定向到哪里。
```

### 1.8 Bootloader 和 App 的启动过程

STM32 程序不是一上电就直接进入 `main()`，而是先进入 `Reset_Handler`。

Bootloader 启动过程：

```text
上电
  ↓
CPU 从 0x08000000 读取 MSP
  ↓
CPU 从 0x08000004 读取 Bootloader Reset_Handler
  ↓
SystemInit
  ↓
__main
  ↓
Bootloader main()
```

App 启动过程：

```text
Bootloader 读取 App 向量表
  ↓
设置 MSP = App 起始地址处保存的栈顶值
  ↓
设置 SCB->VTOR = App 起始地址
  ↓
跳转到 App Reset_Handler
  ↓
SystemInit
  ↓
__main
  ↓
App main()
```

`__main` 不是用户写的 `main()`，它是编译器启动库入口，负责复制 RW-data、清零 ZI-data，然后才进入用户 `main()`。

### 1.9 程序镜像里的 Code、RO、RW、ZI

Keil 编译后常见输出：

```text
Program Size: Code=xxx RO-data=xxx RW-data=xxx ZI-data=xxx
```

各部分作用：

| 名称 | 作用 | 占 Flash | 占 RAM |
|---|---|---|---|
| Code | 函数编译后的机器指令 | 是 | 否 |
| RO-data | const 常量、字符串常量、查表数据 | 是 | 否 |
| RW-data | 有初始值且运行时会修改的变量 | 是 | 是 |
| ZI-data | 未显式初始化或初始化为 0 的变量 | 否 | 是 |

Flash 占用估算：

```text
Code + RO-data + RW-data
```

RAM 占用估算：

```text
RW-data + ZI-data + Stack + Heap
```

判断 Bootloader 是否超过 64KB 时，重点看 Flash 占用，也就是 `Code + RO-data + RW-data`。

### 1.10 SRAM 在 IAP 里的作用

SRAM 起始地址通常是：

```text
0x20000000
```

SRAM 用来放运行时数据，例如：

- 栈。
- 堆。
- 全局变量。
- 串口接收缓冲区。
- 临时固件数据。

教程图里出现的 `0x20001000`，通常表示 Bootloader 在 SRAM 中预留一块区域，临时接收 App 数据。

但实际项目里不一定要把整个 App 都放进 SRAM。更常见做法是：

```text
串口收到一包数据
  ↓
放入 SRAM 接收缓冲区
  ↓
立刻写入 Flash
  ↓
继续接收下一包
```

一句话记忆：

```text
Flash 决定程序放在哪里，SRAM 决定运行时数据临时放在哪里。
```

### 1.11 为什么 IAP 常用 bin 文件

IAP 常用 `.bin`，因为 `.bin` 是纯二进制数据，Bootloader 可以从固定 App 地址开始顺序写入。

```text
bin = 纯固件内容
hex = 固件内容 + 地址信息 + 文本记录格式 + 行校验
```

`.bin` 写入逻辑简单：

```text
收到第 0 字节 -> 写到 APP_ADDR + 0
收到第 1 字节 -> 写到 APP_ADDR + 1
收到第 N 字节 -> 写到 APP_ADDR + N
```

`.hex` 则需要解析文本记录、地址、类型和校验，更适合下载器或量产烧录工具。

产品化 IAP 中更常见的是：

```text
固件头 + app.bin + CRC + 签名
```

固件头里可以放：

- magic 标识。
- 固件大小。
- 目标地址。
- 版本号。
- CRC32。
- 硬件型号。
- 签名或加密信息。

### 1.12 外置 Flash 暂存升级包方案

如果 App 比较大，内部 Flash 放不下 `Bootloader + App A + App B`，可以使用外置 Flash 暂存新固件。

结构：

```text
内部 Flash：
Bootloader + 当前运行 App(A)

外置 Flash：
暂存新固件 App(B)
```

升级流程：

```text
1. App A 正常运行
2. 接收新固件 B
3. 先写到外置 Flash
4. 接收完成后校验 B
5. 写入升级标志
6. 复位进入 Bootloader
7. Bootloader 从外置 Flash 读取 B
8. 擦除内部 Flash 的 A 区
9. 把 B 写入 A 区
10. 校验内部 Flash 中的新 App
11. 跳转运行新的 App
```

这个方案不算严格的内部 A/B 双分区，更准确叫：

```text
单 App 区 + 外置 Flash 下载暂存区
```

优点：

- 节省内部 Flash。
- 下载失败不会破坏当前 App。
- 新固件校验通过后才覆盖旧 App。

风险点：

- 覆盖 A 的过程中如果断电，内部 App 可能不完整。
- 必须保证 Bootloader 不被擦除。
- 外置 Flash 中的 B 要保留到安装成功后再清除。
- 需要升级状态机支持断电恢复。

推荐状态：

```text
IDLE
DOWNLOADING
DOWNLOAD_OK
INSTALLING
INSTALL_OK
INSTALL_FAIL
```

### 1.13 Bootloader 为什么常用内部晶振

Bootloader 可以用外部晶振 HSE，但很多项目会优先用内部晶振 HSI。

原因是：

```text
Bootloader 是救命程序，越少依赖外部器件越可靠。
```

HSI 的优点：

- 芯片内部自带。
- 上电即可使用。
- 不依赖外部晶振和负载电容。
- 配置简单。
- 外部晶振损坏时仍然有机会进入升级模式。

如果 Bootloader 要使用 USB、以太网或高精度通信，可能仍然需要 HSE。但如果只是串口、按键、LED、Flash 擦写，HSI 通常已经够用。

### 1.14 本章核心结论

今天学习后，可以把 IAP 主线概括为：

```text
1. 先通过 ICP 把 Bootloader 烧到 0x08000000。
2. BOOT0 默认拉低，让 STM32 每次上电都先运行 Bootloader。
3. App 不再放 0x08000000，而是放到 0x08010000 或 0x08020000。
4. App 的向量表必须跟着 App 起始地址偏移。
5. Bootloader 接收 app.bin，擦除 App 区，写入新 App。
6. 写完后做 CRC 或完整性校验。
7. 校验通过后，Bootloader 设置 MSP 和 SCB->VTOR，跳转 App Reset_Handler。
8. App 最终进入自己的 main() 正常运行。
```

最容易出错的点：

- App 链接地址没改。
- App 向量表偏移没改。
- Bootloader 擦错 Flash 扇区。
- bin 文件生成方式不对。
- Bootloader 跳 App 前没有正确设置 MSP 和 `SCB->VTOR`。
- CRC 计算范围和写入范围不一致。
- 升级中断电后没有状态恢复机制。

本章记忆口诀：

```text
先烧 Bootloader，再偏移 App；
先校验固件，再擦写 Flash；
先切栈和向量表，再跳 App。
```

## 2. 第二章：最小 Bootloader 跳转 App 实验

本章记录第一次真正跑通的 IAP 基础实验：暂时不做固件接收、Flash 擦写和 CRC，只验证 Bootloader 能否识别 App，并从 Bootloader 正确跳转到 App。

这一章的核心结论：

```text
最小 IAP 地基 = Bootloader 先启动 + App 地址偏移 + 向量表偏移 + 判断合法 App + 切栈/切向量表跳转。
```

### 2.1 实验目标

本次实验只做一件事：

```text
Bootloader 从 0x08000000 启动，然后跳转到 0x08020000 的 App。
```

暂时不做：

- 串口接收 `app.bin`。
- 擦除 App 区。
- 写入 App 区。
- CRC 校验。
- 升级状态机。

先把跳转跑通，是因为后续 IAP 升级的最后一步也是跳 App。如果这一关没过，前面的接收和写入即使成功，最终也跑不起来。

### 2.2 当前工程结构

当前有两个独立工程：

```text
E:\esp32\iap\iap_bootloadr
E:\esp32\iap\iap_app
```

工程角色：

| 工程 | 作用 | 起始地址 |
|---|---|---:|
| `iap_bootloadr` | Bootloader，负责判断和跳转 | `0x08000000` |
| `iap_app` | App，负责验证跳转后是否正常运行 | `0x08020000` |

本实验使用 128KB Bootloader 分区：

```text
Bootloader: 0x08000000 - 0x0801FFFF
App:        0x08020000 - 0x080FFFFF
```

### 2.3 App 工程必须修改的地方

App 不能再按普通工程放在 `0x08000000`，因为这个地址已经给 Bootloader 使用。

Keil 中 App 的 IROM1 配置：

```text
IROM1 Start: 0x08020000
IROM1 Size:  0x000E0000
```

Keil 可能显示为：

```text
0xE0000
```

这是正常的，前导 0 会被省略：

```text
0x000E0000 = 0xE0000 = 896KB
```

`.map` 文件中已经验证：

```text
Execution Region ER_IROM1
Exec base: 0x08020000
Max:       0x000e0000
```

说明 App 已经链接到 `0x08020000`。

### 2.4 App 向量表偏移

App 的向量表必须跟着 App 起始地址一起偏移。

在 App 的 `system_stm32f4xx.c` 中启用：

```c
#define USER_VECT_TAB_ADDRESS
```

并设置：

```c
#define VECT_TAB_OFFSET 0x00020000U
```

最终效果：

```c
SCB->VTOR = 0x08020000;
```

如果不改这个，App 跳转后可能能进 `main()`，但一旦发生 SysTick、串口、按键等中断，就可能跑去 Bootloader 的向量表，导致异常。

### 2.5 App 可观察现象

为了证明 Bootloader 确实跳进了 App，App 中加入 LED 翻转：

```c
HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_10);
HAL_Delay(1000);
```

这样跳转成功后，可以看到 App 的 PF10 LED 按 1 秒节奏翻转。

这一步很重要。没有可观察现象时，很难判断是没跳过去、跳过去卡住，还是已经正常运行但没有输出。

### 2.6 Bootloader 地址宏

Bootloader 中使用的关键地址：

```c
#define BOOTLOADER_ADDR      0x08000000U
#define APP_ADDR             0x08020000U
#define FLASH_END_ADDR       0x08100000U
#define SRAM_START_ADDR      0x20000000U
#define SRAM_END_ADDR        0x20020000U
```

含义：

| 宏 | 作用 |
|---|---|
| `BOOTLOADER_ADDR` | Bootloader 起始地址 |
| `APP_ADDR` | App 起始地址，也是 App 向量表地址 |
| `FLASH_END_ADDR` | 内部 Flash 结束边界的下一个地址 |
| `SRAM_START_ADDR` | SRAM 起始地址 |
| `SRAM_END_ADDR` | SRAM 结束边界的下一个地址 |

这些宏是 Bootloader 的地址地图，用来判断 App 是否存在、Flash 写入是否越界、跳转地址是否合法。

### 2.7 App 合法性判断

Bootloader 判断 App 是否存在，不是看文件名，也不是看工程名，而是读 App 向量表最前面的两个值：

```text
APP_ADDR + 0：App 初始栈顶 MSP
APP_ADDR + 4：App Reset_Handler
```

当前实验读到：

```text
app_msp   = 0x200006B8
app_reset = 0x08020229
```

判断结果：

```text
0x200006B8 在 SRAM 范围内
0x08020229 在 App Flash 范围内
```

所以 Bootloader 能判断 App 合法。

判断伪代码：

```c
static uint8_t boot_app_is_valid(uint32_t app_addr)
{
    uint32_t app_msp = *(__IO uint32_t *)app_addr;
    uint32_t app_reset = *(__IO uint32_t *)(app_addr + 4U);

    if ((app_msp < SRAM_START_ADDR) || (app_msp >= SRAM_END_ADDR)) {
        return 0;
    }

    if ((app_reset < APP_ADDR) || (app_reset >= FLASH_END_ADDR)) {
        return 0;
    }

    return 1;
}
```

注意：这个判断只能说明 App 入口地址看起来合理，不能证明固件完整。后续完整 IAP 还需要 CRC 或签名校验。

### 2.8 按键触发跳转

本次实验使用按键触发跳转，而不是上电自动跳转。

PE3 使用上拉时，常见按键逻辑是：

```text
未按下：GPIO_PIN_SET，高电平
按下：  GPIO_PIN_RESET，低电平
```

判断按下：

```c
if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)
{
    HAL_Delay(100);
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET) {
        boot_jump_to_app(APP_ADDR);
    }
}
```

这里 `HAL_Delay(100)` 是简单按键消抖。

### 2.9 Bootloader 跳转 App 的核心步骤

Bootloader 跳 App 不是直接跳 `main()`，而是跳到 App 的 `Reset_Handler`。

跳转流程：

```text
1. 读取 App MSP
2. 读取 App Reset_Handler
3. 关闭全局中断
4. 反初始化 Bootloader 使用过的串口
5. HAL_DeInit 清理 HAL 状态
6. 停止 SysTick
7. 清理 NVIC 中断使能和挂起标志
8. 设置 SCB->VTOR = APP_ADDR
9. 设置 MSP = App MSP
10. 重新打开全局中断
11. 跳转 App Reset_Handler
```

本次实验里，`__enable_irq()` 很关键：

```c
SCB->VTOR = app_addr;
__set_MSP(app_msp);
__enable_irq();
app_entry();
```

如果跳转前关闭了全局中断，但跳 App 前没有重新打开，App 中的 SysTick 和 `HAL_Delay()` 可能不会工作。

### 2.10 函数指针的作用

跳转代码里有：

```c
typedef void (*boot_app_entry_t)(void);
```

它的意思是定义一个函数指针类型：

```text
指向一个无参数、无返回值的函数。
```

App 的 `Reset_Handler` 本质上就是：

```c
void Reset_Handler(void);
```

所以可以把 App 向量表中的 Reset_Handler 地址转成函数指针：

```c
boot_app_entry_t app_entry = (boot_app_entry_t)app_reset;
```

再调用：

```c
app_entry();
```

这里的 `app_entry()` 不需要自己实现，它只是一个函数指针变量，真正跳转到的是 App 工程启动文件里的 `Reset_Handler`。

### 2.11 为什么复位后又进入 Bootloader

复位后重新进入 Bootloader 是正常现象。

原因：

```text
STM32 上电或复位时，只会从 0x08000000 启动。
```

当前分区中：

```text
0x08000000 是 Bootloader
0x08020000 是 App
```

所以每次复位都会先运行 Bootloader。真正产品里的逻辑通常是：

```text
复位
  ↓
进入 Bootloader
  ↓
判断是否需要升级
  ↓
不升级：自动跳 App
  ↓
升级：留在 Bootloader
```

用户看起来像是直接进入 App，其实 Bootloader 已经快速运行过一遍。

### 2.12 本章实验结果

本次已经跑通：

- [x] Bootloader 能从 `0x08000000` 启动。
- [x] Bootloader 串口能打印启动信息。
- [x] App 已链接到 `0x08020000`。
- [x] App 向量表偏移设置为 `0x00020000`。
- [x] Bootloader 能读到 App MSP 和 Reset_Handler。
- [x] Bootloader 能判断 App 合法。
- [x] 按键低电平触发跳转。
- [x] Bootloader 能切换 MSP 和 `SCB->VTOR`。
- [x] App 跳转后能运行 LED 翻转。
- [x] 复位后重新进入 Bootloader，符合 IAP 启动机制。

本章记忆口诀：

```text
每次复位先进 Bootloader；
App 不在零地址，向量表也要偏移；
跳 App 前先清现场，再切 MSP 和 VTOR；
最后跳 Reset_Handler，不是跳 main。
```

## 3. 从 mini IAP 到 mOTA 的实战路线

当前学习路线调整为：先自己写一个最小可用的 mini IAP，再带着问题去看 mOTA。这样不是一开始就啃大神工程，而是先把核心链路在自己手里跑通。

总路线：

```text
1. 自己写一个能跳转 APP 的 Bootloader
2. 自己写一个串口升级 APP 的 mini IAP
3. 自己加 CRC
4. 看 mOTA 的分区设计
5. 看 mOTA 的固件包格式
6. 看 mOTA 的断电保护
7. 看 mOTA 的代码分层
8. 把自己的 mini 版重构成更工程化的版本
```

一句话记忆：

```text
先自己做小闭环，再看成熟工程为什么那样设计。
```

### 第一步：自己写最小 Bootloader

目标：只搞懂 Bootloader 怎么启动、怎么判断 App、怎么跳转 App。

只实现这几个功能：

1. 上电进入 Bootloader。
2. 判断 APP 地址有没有有效程序。
3. 有 APP 就跳转。
4. 没 APP 就停在 Bootloader。

暂时不做：

- OTA。
- 串口接收 bin。
- Flash 写入。
- CRC。
- 固件包。
- 断电保护。

核心知识点：

| 知识点 | 作用 |
|---|---|
| MSP 主堆栈指针 | App 启动后使用自己的栈 |
| Reset_Handler | App 真正的复位入口 |
| `SCB->VTOR` | 切换到 App 的异常向量表 |
| APP 起始地址 | Bootloader 从这里读取 App 向量表 |
| 关闭中断 | 跳转时避免 Bootloader 中断干扰 App |

本项目当前状态：

```text
已完成。
Bootloader: 0x08000000
App:        0x08020000
```

已验证：

- Bootloader 能启动。
- Bootloader 能读取 App 的 MSP 和 Reset_Handler。
- App 合法时能按键跳转。
- App 跳转后 PF10 LED 能运行。

这一关特别重要，因为 OTA 的根就在这里。后续无论固件怎么传、协议怎么封装、分区怎么设计，最终都要回到这一步：

```text
校验 App -> 切 MSP -> 切 VTOR -> 跳 Reset_Handler
```

### 第二步：加串口接收 bin

目标：做最笨但能跑通的串口升级。

最小流程：

```text
PC 上位机通过串口发 bin
  ↓
STM32 Bootloader 接收数据
  ↓
写入 APP 分区
  ↓
写完后跳转 APP
```

这一阶段先不急着用 YModem。先自己定义一个简单协议，理解“固件怎么被拆包、怎么被接收、怎么被写入”。

建议协议：

```text
AA 55 + CMD + LEN + DATA + CRC
```

字段含义：

| 字段 | 作用 |
|---|---|
| `AA 55` | 帧头，用来找一包数据的开始 |
| `CMD` | 命令，例如开始、数据、结束 |
| `LEN` | 本包数据长度 |
| `DATA` | 固件数据或命令参数 |
| `CRC` | 本包校验，先可用简单校验，后续换 CRC32 |

建议先拆成三个小实验：

1. 串口收到 1 个字节并打印。
2. 串口收到一包固定长度数据并打印长度。
3. 串口按协议解析 `AA 55 + CMD + LEN + DATA + CRC`。

再进入真正写 Flash：

```text
START：接收固件总长度
DATA：分包接收并写入 App 区
END：结束接收并尝试跳转
```

### 第三步：加 CRC 校验

目标：让 mini IAP 从“能用”变成“稍微靠谱”。

最小校验流程：

```text
接收固件
  ↓
计算 CRC
  ↓
和上位机发来的 CRC 对比
  ↓
正确才写入 / 跳转
```

这里有两种做法：

| 做法 | 特点 |
|---|---|
| 接收时边收边写，最后对 Flash 计算 CRC | 更接近真实升级 |
| 先接收到缓冲区或外部 Flash，校验后再写 | 更安全，但需要额外空间 |

初学阶段可以先做：

```text
接收一包 -> 写一包 -> 最后对 App 区整体算 CRC
```

需要特别注意：

- 上位机和 Bootloader 的 CRC 算法必须一致。
- CRC 计算范围必须一致。
- 固件长度必须参与边界检查。
- CRC 失败不能跳转 App。

### 第四步：开始看 mOTA 的分区设计

完成 mini IAP 后再看 mOTA，重点看它怎么分区。

带着这些问题看：

```text
我现在只有 Bootloader + 单 App，mOTA 为什么要做多分区？
我现在 App 地址写死，mOTA 怎么配置不同分区？
我现在直接擦写 App 区，mOTA 怎么避免误擦？
```

重点关注：

- Bootloader 区。
- App 运行区。
- 下载区。
- 参数区。
- 备份区。
- A/B 或多分区切换策略。

### 第五步：看 mOTA 的固件包格式

完成裸 `bin` 升级后，再看 mOTA 为什么不只传裸 bin。

带着这些问题看：

```text
我现在只是裸 bin，它为什么要做 fpk 固件包？
固件包里除了程序数据，还放了什么？
固件长度、版本号、目标地址、CRC 放在哪里？
```

裸 bin 的问题：

- 不知道目标硬件型号。
- 不知道版本号。
- 不知道固件长度。
- 不知道目标地址。
- 不方便做签名、加密、防回滚。

固件包通常会包含：

```text
固件头 + 固件数据 + CRC/签名
```

固件头常见字段：

- magic 标志。
- 固件大小。
- 固件版本。
- 目标芯片或硬件型号。
- 目标写入地址。
- CRC32。
- 包格式版本。
- 加密或签名标志。

### 第六步：看 mOTA 的断电保护

完成 mini IAP 后会发现一个问题：

```text
如果升级时断电，App 可能被写坏。
```

这时候再看 mOTA 的断电保护，理解会快很多。

带着这些问题看：

```text
我现在升级中断电会变砖，它怎么做断电保护？
它怎么知道上次升级进行到哪一步？
它怎么判断该继续安装、回滚，还是停在 Bootloader？
```

重点关注状态机：

```text
IDLE
DOWNLOADING
DOWNLOAD_OK
INSTALLING
INSTALL_OK
INSTALL_FAIL
APP_CONFIRMED
```

关键原则：

- 不能只靠“某个区域有数据”判断固件可用。
- 必须有状态标志。
- 必须有 CRC 或签名。
- 安装过程中断电后，Bootloader 必须知道下一步该做什么。

### 第七步：看 mOTA 的代码分层

完成 mini 版后，再看它为什么要分层。

带着这些问题看：

```text
我现在直接调用 HAL_FLASH，它为什么封装 bsp_flash？
我现在协议、Flash、跳转都写在 main.c，它为什么拆成多个模块？
我现在只支持串口，它怎么扩展到别的通信方式？
```

建议观察这些层：

| 层 | 作用 |
|---|---|
| `bsp_flash` | 屏蔽不同芯片 Flash 擦写差异 |
| `bsp_uart` / `bsp_can` | 屏蔽通信外设差异 |
| protocol | 处理升级协议 |
| package | 解析固件包 |
| boot | 启动判断与跳转 |
| app_manage | 分区、版本、状态管理 |

这一步的目标不是照抄，而是理解工程化代码为什么要这样拆。

### 第八步：把 mini 版重构成工程化版本

当 mini IAP 跑通，并且看懂 mOTA 的设计后，再回头重构自己的版本。

重构方向：

- 把 Flash 操作从 `main.c` 拆到 `boot_flash.c`。
- 把跳转逻辑拆到 `boot_jump.c`。
- 把串口协议拆到 `boot_protocol.c`。
- 把 CRC 拆到 `boot_crc.c`。
- 把升级状态拆到 `boot_param.c`。
- 把地址和分区集中到 `boot_config.h`。

目标结构：

```text
Bootloader/
  App/
    boot_config.h
    boot_jump.c
    boot_jump.h
    boot_flash.c
    boot_flash.h
    boot_protocol.c
    boot_protocol.h
    boot_crc.c
    boot_crc.h
    boot_param.c
    boot_param.h
```

最终目标不是一开始就写得复杂，而是：

```text
先写得出来，再写得可靠，最后写得漂亮。
```

## 4. 推荐工程结构

如果从零搭建，建议这样组织：

```text
iap_project/
  Bootloader/
    Core/
    Drivers/
    App/
      boot_jump.c
      boot_flash.c
      boot_protocol.c
      boot_crc.c
      boot_param.c
  Application/
    Core/
    Drivers/
    App/
      app_main.c
      app_upgrade.c
  Tools/
    send_firmware.py
  docs/
    iap升级.md
```

如果你用 Keil，也可以是两个独立工程：

```text
Bootloader.uvprojx
Application.uvprojx
```

关键点不是目录长什么样，而是 Bootloader 和 App 的职责要清楚。

## 5. Bootloader 核心职责

Bootloader 应该做：

- 初始化最小外设：时钟、串口、Flash、按键、LED。
- 判断是否进入升级模式。
- 接收新固件。
- 擦除 App 区。
- 写入 App 区。
- 校验固件完整性。
- 保存升级结果和版本信息。
- 跳转到 App。

Bootloader 不建议做：

- 放复杂业务逻辑。
- 长时间依赖动态内存。
- 依赖太多不稳定外设。
- 频繁修改自身所在 Flash 区。
- 在升级失败时强行跳转 App。

## 6. App 核心职责

App 应该做：

- 正常业务功能。
- 提供版本号。
- 必要时设置升级请求标志。
- 复位进入 Bootloader。
- 可选：上报运行健康状态。

App 不建议做：

- 擦写 Bootloader 区。
- 自己搬运完整升级固件到 App 区覆盖自身。
- 忽略 Bootloader 定义的参数结构。

## 7. 跳转 App 的关键流程

Bootloader 跳转 App 的典型步骤：

1. 关闭全局中断或禁用已启用中断。
2. 停止 SysTick。
3. 反初始化已使用外设。
4. 检查 App 初始栈地址是否合法。
5. 设置 MSP 为 App 向量表第一个 word。
6. 设置向量表偏移到 App 起始地址。
7. 取 App Reset_Handler 并跳转。

伪代码：

```c
#define APP_ADDR 0x08020000U

typedef void (*app_entry_t)(void);

void boot_jump_to_app(void)
{
    uint32_t app_msp = *(__IO uint32_t *)APP_ADDR;
    uint32_t app_reset = *(__IO uint32_t *)(APP_ADDR + 4U);

    if ((app_msp & 0x2FFE0000U) != 0x20000000U) {
        return;
    }

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    SCB->VTOR = APP_ADDR;
    __set_MSP(app_msp);

    ((app_entry_t)app_reset)();
}
```

注意：

- 这只是学习伪代码，实际项目要结合 HAL 反初始化、中断清理和芯片 SRAM 大小调整。
- App 工程也要配置向量表偏移，否则中断可能仍然跳到 Bootloader 的中断表。

## 8. 固件头部建议

为了让 Bootloader 判断固件是否可信，建议给固件加一个头部。

示例结构：

```c
typedef struct
{
    uint32_t magic;
    uint32_t header_size;
    uint32_t firmware_size;
    uint32_t firmware_crc32;
    uint32_t version;
    uint32_t build_time;
    uint32_t target_addr;
    uint32_t reserved[9];
} firmware_header_t;
```

字段含义：

| 字段 | 作用 |
|---|---|
| `magic` | 判断是否为合法固件 |
| `header_size` | 方便以后扩展头部 |
| `firmware_size` | 判断是否越界 |
| `firmware_crc32` | 校验完整性 |
| `version` | 版本管理和防回滚 |
| `target_addr` | 判断是否写入正确分区 |

初学阶段可以先不做复杂打包，只用 PC 工具单独发送大小和 CRC。等主链路跑通后，再把固件头部固化下来。

## 9. 常见问题与排查

### 跳转 App 后卡死

可能原因：

- App 起始地址没有修改。
- App 向量表偏移没有修改。
- Bootloader 没有关闭 SysTick。
- Bootloader 中断没有清理。
- MSP 设置错误。
- App Reset_Handler 地址不合法。

排查方法：

- 读取 `APP_ADDR` 处第一个 word，确认是否像 SRAM 地址。
- 读取 `APP_ADDR + 4`，确认是否像 Flash 地址。
- 在 App 的 `main()` 开头点灯或串口打印。
- 单独下载 App 到偏移地址验证是否能运行。

### Flash 写入失败

可能原因：

- 写入前没有擦除。
- 写入地址没有对齐。
- 写入长度越界。
- Flash 未解锁。
- 电压范围配置不对。
- 擦除了 Bootloader 所在扇区。

排查方法：

- 每次擦除和写入都打印地址、长度和返回值。
- 先用固定数组写入，再接入串口协议。
- 用调试器查看目标地址内容。

### 升级后第一次能跑，第二次失败

可能原因：

- 参数区和 App 区重叠。
- 写入地址递增计算错误。
- 上一次固件长度比这一次长，尾部旧数据影响校验。
- CRC 计算范围不统一。

排查方法：

- 固件大小、写入大小、CRC 计算范围必须统一。
- 升级前完整擦除 App 使用到的扇区。
- 打印每包序号和累计写入长度。

## 10. 建议实验清单

- [ ] 实验 1：Bootloader 延时跳转 App。
- [ ] 实验 2：按键控制是否停留在 Bootloader。
- [ ] 实验 3：App 主动写升级标志并软复位。
- [ ] 实验 4：Bootloader 擦除 App 区。
- [ ] 实验 5：Bootloader 写入固定测试数据。
- [ ] 实验 6：串口接收固件但不写 Flash，只计算 CRC。
- [ ] 实验 7：串口接收固件并写入 Flash。
- [ ] 实验 8：升级完成后校验 CRC 并跳转。
- [ ] 实验 9：升级中途断电，确认不会启动坏 App。
- [ ] 实验 10：加入版本号和升级结果记录。

## 11. 推荐学习顺序

当前学习顺序以“先自己跑通 mini IAP，再看 mOTA”为主线。

第一阶段：最小 Bootloader

- 目标：上电进入 Bootloader，判断 App，跳转 App。
- 状态：已完成。
- 关键知识：MSP、Reset_Handler、`SCB->VTOR`、App 起始地址、关闭/恢复中断。
- 验收：App 放在 `0x08020000`，Bootloader 能按键跳转，App LED 正常翻转。

第二阶段：最小串口接收

- 目标：先不写 Flash，只让 Bootloader 能收到 PC 发来的串口数据。
- 实验 1：USART2 收 1 个字节，USART1 打印。
- 实验 2：USART2 收固定长度数据，打印长度和内容。
- 实验 3：解析简单帧头 `AA 55 + CMD + LEN + DATA + CRC`。
- 验收：能稳定接收数据包，能识别帧头、命令和长度。

第三阶段：串口写入 App 分区

- 目标：通过串口接收 `app.bin` 并写入 App 区。
- 先做最笨版本：不做复杂协议，只保证能按顺序写入。
- 再加入 `START / DATA / END` 三类命令。
- 验收：接收完成后，Bootloader 能跳转到新 App。

第四阶段：加入 CRC

- 目标：让 mini IAP 从“能跑”变成“稍微靠谱”。
- 上位机发送固件长度和 CRC。
- Bootloader 接收后计算 CRC。
- CRC 正确才允许跳转。
- CRC 错误则停留 Bootloader。
- 验收：错误固件不会被启动。

第五阶段：回头看 mOTA 分区设计

- 目标：理解成熟工程为什么要分区。
- 重点看 Bootloader 区、App 区、下载区、参数区、备份区。
- 对比自己的 mini IAP：为什么单 App 简单但不够安全。
- 验收：能画出 mOTA 的分区图，并说明每个区的作用。

第六阶段：看 mOTA 固件包格式

- 目标：理解为什么不用裸 bin，而要做 fpk 或类似固件包。
- 重点看固件头字段：magic、version、size、target、CRC、签名。
- 对比自己的 mini IAP：裸 bin 缺少哪些信息。
- 验收：能设计一个自己的 `firmware_header_t`。

第七阶段：看 mOTA 断电保护

- 目标：理解升级过程中断电后如何恢复。
- 重点看升级状态机：`DOWNLOADING`、`DOWNLOAD_OK`、`INSTALLING`、`INSTALL_OK`、`APP_CONFIRMED`。
- 对比自己的 mini IAP：什么时候会变砖，怎么避免。
- 验收：能说明断电发生在下载中、写入中、首次启动失败时分别怎么处理。

第八阶段：重构自己的 mini IAP

- 目标：把能跑的实验代码改成更工程化的结构。
- 拆分模块：`boot_jump`、`boot_flash`、`boot_protocol`、`boot_crc`、`boot_param`。
- 把地址、扇区、状态集中到配置文件。
- 验收：功能不变，但代码结构更清晰，后续能扩展外置 Flash、A/B 分区或 mOTA 思路。

## 12. 后续笔记增量记录模板

每次做实验后，把记录补在这里。

### 实验记录模板

```text
日期：
实验目标：
硬件平台：
芯片型号：
开发环境：
Bootloader 地址：
App 地址：
操作步骤：
现象：
结论：
遗留问题：
相关代码：
```

### 排障记录模板

```text
问题现象：
复现条件：
初步判断：
定位过程：
根因：
修复动作：
验证结果：
经验沉淀：
```

## 13. 当前待确认项

- [ ] STM32F407 的具体子型号和 Flash 容量。
- [ ] 使用 Keil、IAR 还是 STM32CubeIDE。
- [ ] 使用 HAL 库、标准库还是裸寄存器。
- [ ] 计划用串口、CAN、以太网、4G 还是 ESP32 作为升级通道。
- [ ] 是否需要保留出厂 App 或 A/B 双分区。
- [ ] 是否需要加密、签名、防回滚。

## 14. 我的学习主线

一句话记忆：

```text
先会跳，再会擦，再会写，再会收，再会校验，最后再谈可靠和安全。
```

IAP 最容易出问题的地方不是协议本身，而是地址、向量表、Flash 擦写边界、异常断电和校验策略。学习时每一步都要能单独验证，不要把串口协议、Flash 写入、跳转 App、CRC 校验全部混在一起同时调。
