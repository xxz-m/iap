---
tags:
  - stm32
  - stm32f407
  - iap
  - bootloader
  - hal
topic: Bootloader 最小设计流程
status: active
created: 2026-05-05
updated: 2026-05-05
---

# Bootloader 最小设计流程

## 1. 当前工程检查结论

当前工程目录：

```text
E:\esp32\iap\iap_bootloadr
```

工程类型：

```text
STM32F407IGTx
CubeMX + HAL
Keil MDK-ARM
```

当前已生成的基础能力：

- 已有 `HAL_Init()`。
- 已有 `SystemClock_Config()`。
- 已有 `MX_GPIO_Init()`。
- 已有 `MX_USART1_UART_Init()`。
- 已有 `MX_USART2_UART_Init()`。
- `USART2` 已开启中断。
- `HAL_FLASH_MODULE_ENABLED` 已启用，可以后续使用 HAL Flash 擦写接口。
- `PF9` 配置为输出，可用于 LED 指示。
- `PE3` 配置为输入，可用于按键或启动模式判断。

当前编译体积：

```text
Total ROM Size: 5536 字节，约 5.41KB
Total RW Size:  1792 字节，约 1.75KB
```

按 64KB Bootloader 预算来看，目前体积很小，完全够用。

当前需要注意的点：

```text
Keil 当前 IROM1 仍配置为完整 1MB：
Start: 0x08000000
Size:  0x00100000
```

学习阶段可以先这样跑基础功能。但后续真正做 IAP 分区时，建议把 Bootloader 的 IROM1 限制为预留区域，例如：

```text
64KB Bootloader:
Start: 0x08000000
Size:  0x00010000

128KB Bootloader:
Start: 0x08000000
Size:  0x00020000
```

当前未发现影响“最小 Bootloader 学习流程”的明显问题。

## 2. 最小 Bootloader 的学习目标

第一版不要急着做完整升级，先完成一个最小闭环：

```text
Bootloader 上电运行
  ↓
打印或点灯证明自己启动
  ↓
判断 App 区是否存在合法程序
  ↓
如果合法，跳转到 App
  ↓
如果不合法，停留在 Bootloader
```

这个阶段暂时不做：

- 串口接收固件。
- Flash 擦除。
- Flash 写入。
- CRC 校验。
- 升级协议。
- 断电恢复。

先把“启动、判断、跳转”跑通，后面再逐步叠加。

## 3. 推荐 Flash 分区

初学建议先用 128KB Bootloader，空间更宽松：

```text
Bootloader:
0x08000000 - 0x0801FFFF

App:
0x08020000 - 0x080FFFFF
```

关键宏可以先这样设计：

```c
#define BOOTLOADER_ADDR      0x08000000U
#define APP_ADDR             0x08020000U
#define FLASH_END_ADDR       0x08100000U
#define SRAM_START_ADDR      0x20000000U
#define SRAM_END_ADDR        0x20020000U
```

说明：

- `APP_ADDR` 是 App 向量表起始地址。
- `APP_ADDR + 0` 保存 App 初始栈顶 MSP。
- `APP_ADDR + 4` 保存 App 复位入口 `Reset_Handler`。
- `SRAM_END_ADDR` 需要根据实际 SRAM 容量确认，当前先按常见 `0x20000000 - 0x2001FFFF` 理解。

如果你想跟课程图一致，也可以用 64KB Bootloader：

```text
Bootloader:
0x08000000 - 0x0800FFFF

App:
0x08010000 - 0x080FFFFF
```

两种都能做。初学更推荐 `0x08020000`，因为 App 从 Sector 5 开始，扇区边界更清楚。

## 4. 第一版 Bootloader 主流程

最小主流程：

```text
main()
  ↓
HAL_Init()
  ↓
SystemClock_Config()
  ↓
MX_GPIO_Init()
  ↓
MX_USARTx_UART_Init()
  ↓
打印 Bootloader 启动信息
  ↓
延时 1 秒，方便观察
  ↓
判断 App 是否合法
  ↓
合法：跳转 App
  ↓
不合法：停留 Bootloader，LED 慢闪
```

伪代码：

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    boot_print("bootloader start\r\n");
    HAL_Delay(1000);

    if (boot_app_is_valid(APP_ADDR)) {
        boot_print("jump to app\r\n");
        HAL_Delay(100);
        boot_jump_to_app(APP_ADDR);
    }

    boot_print("no valid app, stay in bootloader\r\n");

    while (1) {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
        HAL_Delay(500);
    }
}
```

## 5. App 合法性判断

最小判断只看两个值：

```text
APP_ADDR + 0：初始 MSP
APP_ADDR + 4：Reset_Handler
```

判断规则：

```text
MSP 必须落在 SRAM 范围
Reset_Handler 必须落在 App Flash 范围
```

伪代码：

```c
static uint8_t boot_app_is_valid(uint32_t app_addr)
{
    uint32_t app_msp = *(__IO uint32_t *)app_addr;
    uint32_t app_reset = *(__IO uint32_t *)(app_addr + 4U);

    if (app_msp < SRAM_START_ADDR || app_msp > SRAM_END_ADDR) {
        return 0;
    }

    if (app_reset < APP_ADDR || app_reset >= FLASH_END_ADDR) {
        return 0;
    }

    return 1;
}
```

这个判断不能证明 App 一定完整，但能避免明显空 Flash 或跳飞。

空 Flash 常见读数：

```text
0xFFFFFFFF
```

所以没有 App 时，`app_msp` 和 `app_reset` 很可能都是 `0xFFFFFFFF`，判断会失败。

## 6. Bootloader 跳转 App

Bootloader 跳 App 不是直接跳 `main()`，而是跳到 App 的 `Reset_Handler`。

跳转前必须做几件事：

1. 关闭全局中断。
2. 停止 SysTick。
3. 关闭或反初始化 Bootloader 用过的外设。
4. 清理 NVIC 中断使能和挂起标志。
5. 设置 App 向量表地址 `SCB->VTOR`。
6. 设置 MSP 为 App 的初始栈顶。
7. 跳转 App 的 `Reset_Handler`。

伪代码：

```c
typedef void (*boot_app_entry_t)(void);

static void boot_jump_to_app(uint32_t app_addr)
{
    uint32_t app_msp = *(__IO uint32_t *)app_addr;
    uint32_t app_reset = *(__IO uint32_t *)(app_addr + 4U);
    boot_app_entry_t app_entry = (boot_app_entry_t)app_reset;

    __disable_irq();

    HAL_UART_DeInit(&huart1);
    HAL_UART_DeInit(&huart2);
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SCB->VTOR = app_addr;
    __set_MSP(app_msp);

    app_entry();
}
```

注意：

- 这是学习版流程，后续写进工程时要结合你实际启用的外设调整。
- 如果 Bootloader 开了串口中断，跳转前一定要清掉。
- App 工程也必须配置自己的起始地址和向量表偏移。

## 7. App 工程需要配合什么

Bootloader 能跳 App，不代表 App 工程什么都不用改。

App 工程必须改：

```text
IROM1 Start = APP_ADDR
IROM1 Size  = 剩余 App 空间
```

如果选择：

```text
APP_ADDR = 0x08020000
```

则 App 工程需要：

```text
IROM1 Start: 0x08020000
```

同时 App 向量表也要指向 App 起始地址。

如果在 `system_stm32f4xx.c` 中配置：

```c
#define USER_VECT_TAB_ADDRESS
#define VECT_TAB_OFFSET 0x00020000U
```

最终效果应为：

```c
SCB->VTOR = 0x08020000;
```

如果使用 `0x08010000` 作为 App 起始地址：

```c
#define VECT_TAB_OFFSET 0x00010000U
```

## 8. 第一版验收标准

完成最小 Bootloader 后，用下面几项验收：

- [ ] 上电后 Bootloader 能运行。
- [ ] LED 或串口能证明 Bootloader 已启动。
- [ ] App 区为空时，Bootloader 不跳转，停留并闪灯。
- [ ] App 区有合法 App 时，Bootloader 能跳转。
- [ ] App 跳转后能进入自己的 `main()`。
- [ ] App 的 SysTick、串口、按键中断正常。
- [ ] 复位多次后表现一致。

如果这些都通过，说明 IAP 的“启动和跳转地基”已经搭好。

## 9. 第二版再加入 Flash 擦写

第二版目标：

```text
Bootloader 能擦除 App 区指定扇区
Bootloader 能写入固定测试数据
Bootloader 能读回验证
```

暂时仍然不接收固件。

建议顺序：

1. 写一个 `boot_flash_erase_app()`。
2. 写一个 `boot_flash_write_word()` 或 `boot_flash_write_buffer()`。
3. 在 App 起始区以外的测试地址先写固定数据。
4. 读回确认一致。
5. 再测试擦除 App 区。

这一版重点是学会：

```text
解锁 Flash
擦除扇区
写入数据
读回验证
上锁 Flash
```

## 10. 第三版再加入串口接收

第三版目标：

```text
PC 通过串口发送数据
Bootloader 接收数据
Bootloader 先只统计长度，不写 Flash
```

建议先做简单协议：

```text
命令 + 长度 + 数据 + 简单校验
```

先别急着做 YModem。自己先做一个最小协议，理解数据包、ACK、NACK、超时、重发，再上成熟协议会更稳。

## 11. 第四版再加入完整升级

第四版目标：

```text
接收 app.bin
擦除 App 区
写入 App 区
校验
跳转 App
```

这一版才是真正的最小 IAP。

完整流程：

```text
进入升级模式
  ↓
接收固件大小
  ↓
判断是否超过 App 区
  ↓
擦除 App 区
  ↓
分包接收 app.bin
  ↓
分包写入 Flash
  ↓
计算 CRC
  ↓
CRC 正确，写升级成功标志
  ↓
跳转 App
```

## 12. 后续增强方向

最小 IAP 跑通后，再逐步加：

- 固件 CRC32。
- 固件头。
- 版本号。
- 升级标志。
- 按键进入升级。
- App 请求升级。
- 升级超时。
- 断电恢复。
- 外置 Flash 暂存升级包。
- A/B 分区。
- 加密与签名。
- 防回滚。

## 13. 当前阶段记忆口诀

```text
第一步不写升级，先学会跳 App。
第二步不收固件，先学会擦写 Flash。
第三步不急协议，先学会收一包数据。
最后再把接收、擦写、校验、跳转串起来。
```

最小 Bootloader 的核心就三句话：

```text
我先启动。
我判断 App 能不能跑。
能跑我就切栈、切向量表、跳过去。
```
