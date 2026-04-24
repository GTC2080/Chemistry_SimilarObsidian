/**
 * TRUTH_SYSTEM — 全维度个人成长数据模型
 *
 * 等级与经验完全由学习时长驱动（study_sessions 表推导），
 * 升级算法：nextLevelExp = 100 * 1.5^(level-1)
 */

/* ========== 核心类型 ========== */

export interface Attributes {
  science: number;      // 科研与理论推演
  engineering: number;  // 编程与系统架构
  creation: number;     // 宏大叙事与文字创作
  finance: number;      // 量化与逻辑分析
}

export type AttributeKey = keyof Attributes;

export interface TruthState {
  level: number;
  totalExp: number;
  nextLevelExp: number;
  attributes: Attributes;
  /** 各属性累计经验 */
  attributeExp: Attributes;
  /** 上次结算时间戳 (ms) */
  lastSettlement: number;
}

/* ========== 默认状态 ========== */

export function createDefaultState(): TruthState {
  return {
    level: 1,
    totalExp: 0,
    nextLevelExp: 100,
    attributes: { science: 1, engineering: 1, creation: 1, finance: 1 },
    attributeExp: { science: 0, engineering: 0, creation: 0, finance: 0 },
    lastSettlement: Date.now(),
  };
}
