# STM32H755ZIT6U SDMMC 4-bit High-Speed Raw Data Logger Project

## 📌 Project Overview

本專案使用 **STM32H755ZIT6U (NUCLEO-H755ZI-Q)** 搭配  
**DIGILENT Pmod MicroSD 4-bit SDMMC 模組**，實作一套高效能 Raw Data Logger 系統。

本系統目標是：

- 最大化 SD Card 寫入吞吐量
- 完全避免 FATFS File System
- 使用 DMA + Interrupt（禁止 polling）
- 使用 large buffer batch write
- 使用示波器量測完整 write latency

---

# 🎯 System Design Goals

## Functional Requirements

- 每筆資料固定 **64 bytes**
  - 52 bytes：payload（固定測試字串）
  - 2 bytes：`\r\n`
  - 10 bytes：padding (0xAA)
- 建立 **32KB buffer**
- 每次寫入：
  - 32KB = 512 records
- Raw block write to SD card（不使用 FATFS）

## Performance Requirements

- SDMMC 4-bit mode
- DMA + IDMA transfer
- Interrupt-driven (NO polling)
- 1000 次連續寫入測試
- 每次間隔 10ms
- PA6 GPIO pulse 用於示波器測量寫入時間

---

# ⚙️ STM32CubeMX Configuration Guide

---

## 🧠 1. CPU Cache Settings (VERY IMPORTANT)

### Enable:

- ✔ I-Cache: ENABLED
- ✔ D-Cache: ENABLED

### Reason:

- 提升 AXI bus throughput
- DMA burst performance 必須依賴 cache system
- SDMMC high speed 必須 cache support

---

## 🧱 2. MPU Configuration (CRITICAL FOR DMA)

### Region 0 Settings (ONLY REQUIRED REGION)

| Parameter | Value |
|----------|------|
| Base Address | `0x24000000` |
| Size | 32KB |
| Access | Full Access |
| Execution | Disabled |
| Cacheable | ❌ DISABLED |
| Bufferable | ❌ DISABLED |
| Shareable | ENABLED |

### Key Concept:

> DMA buffer MUST be non-cacheable memory

### Why:

- 避免 CPU cache 與 DMA data mismatch
- SD card must receive real memory content
- Prevent corrupted write blocks

---

## ⏱️ 3. Clock Configuration (CRITICAL PERFORMANCE FACTOR)

### Final Clock Tree Result

| Clock Domain | Frequency |
|-------------|----------|
| SYSCLK | 225 MHz |
| AHB | 225 MHz |
| APB1/2/3/4 | 112.5 MHz |
| SDMMC1 clock | 150 MHz |

---

### PLL Summary

- PLL Source: HSE
- PLL1 used for SYSCLK generation
- Peripheral clocks derived from PLLQ / bus dividers

---

### Important Notes

- SDMMC clock = 150 MHz (stable high-speed mode)
- Higher SYSCLK = higher DMA efficiency
- Bottleneck is SD card internal NAND write latency

---

## 💾 4. SDMMC Peripheral Configuration

### Settings:

- Mode: SD 4-bit Wide Bus
- Clock Edge: Rising
- Hardware Flow Control: ENABLED
- Clock Divider: 0 (max speed)
- DMA: ENABLED (IDMA mode)
- Interrupt: ENABLED (NO polling allowed)

---

## 🔄 5. DMA Configuration (MANDATORY)

### DMA Requirements:

- SDMMC1 DMA enabled
- Interrupt-driven transfer only
- Priority: HIGH / VERY HIGH
- Burst mode enabled internally (IDMA)

### Design Rule:

> ❌ NEVER use polling mode  
> ✔ ALWAYS use DMA + IRQ callback

---

## 🧠 6. Memory Architecture (IMPORTANT)

### Buffer Location:

- SRAM region: `0x24000000`

### Buffer Design:

- 32KB aligned buffer
- Used for batch write only
- DMA source buffer

### Data Flow:

Application → 32KB Buffer → DMA → SDMMC → SD Card

---

## 🧪 7. GPIO Timing Measurement (PA6)

### Configuration:

- PA6 = GPIO Output Push-Pull
- Speed: Very High

### Usage:

```c
PA6 = HIGH → Start SD write
PA6 = LOW  → End SD write
