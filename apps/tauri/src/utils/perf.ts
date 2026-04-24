/**
 * Lightweight performance instrumentation for key user-facing paths.
 *
 * Uses the browser Performance API (mark/measure) so entries are also
 * visible in DevTools "Performance" recordings.
 *
 * Usage:
 *   const end = perf.start("vault-open");
 *   await doExpensiveWork();
 *   end();                              // logs duration + creates a measure
 *
 *   perf.mark("app-interactive");       // one-shot timestamp
 *
 *   perf.getSummary();                  // 获取所有指标的统计摘要
 */

const PREFIX = "[perf]";

/** 指标记录：每个 label 保留最近 N 次测量 */
const HISTORY_LIMIT = 20;
const history = new Map<string, number[]>();

function recordDuration(label: string, duration: number) {
  let list = history.get(label);
  if (!list) {
    list = [];
    history.set(label, list);
  }
  list.push(duration);
  if (list.length > HISTORY_LIMIT) {
    list.shift();
  }
}

/** Start a named timer. Returns a function that stops and logs it. */
function start(label: string): () => number {
  const markName = `perf:${label}:start`;
  performance.mark(markName);
  const t0 = performance.now();

  return () => {
    const duration = performance.now() - t0;
    const endMark = `perf:${label}:end`;
    performance.mark(endMark);
    try {
      performance.measure(`perf:${label}`, markName, endMark);
    } catch {
      // measure may throw if marks were cleared
    }
    recordDuration(label, duration);
    console.info(`${PREFIX} ${label}: ${duration.toFixed(1)}ms`);
    return duration;
  };
}

/** Drop a one-shot timestamp mark. */
function mark(label: string) {
  performance.mark(`perf:${label}`);
  console.info(`${PREFIX} ${label} @ ${performance.now().toFixed(1)}ms`);
}

/** 基线指标 */
interface PerfMetric {
  label: string;
  count: number;
  last: number;
  avg: number;
  min: number;
  max: number;
  p90: number;
}

/** 获取所有已记录指标的统计摘要 */
function getSummary(): PerfMetric[] {
  const metrics: PerfMetric[] = [];

  for (const [label, durations] of history) {
    if (durations.length === 0) continue;

    const sorted = [...durations].sort((a, b) => a - b);
    const sum = sorted.reduce((a, b) => a + b, 0);
    const p90Index = Math.min(Math.floor(sorted.length * 0.9), sorted.length - 1);

    metrics.push({
      label,
      count: sorted.length,
      last: sorted[sorted.length - 1],
      avg: sum / sorted.length,
      min: sorted[0],
      max: sorted[sorted.length - 1],
      p90: sorted[p90Index],
    });
  }

  return metrics.sort((a, b) => a.label.localeCompare(b.label));
}

/** 在控制台打印格式化的性能基线表 */
function printBaseline() {
  const metrics = getSummary();
  if (metrics.length === 0) {
    console.info(`${PREFIX} 暂无指标数据`);
    return;
  }

  console.group(`${PREFIX} 性能基线`);
  console.table(
    metrics.map(m => ({
      指标: m.label,
      次数: m.count,
      最近: `${m.last.toFixed(1)}ms`,
      平均: `${m.avg.toFixed(1)}ms`,
      最小: `${m.min.toFixed(1)}ms`,
      最大: `${m.max.toFixed(1)}ms`,
      P90: `${m.p90.toFixed(1)}ms`,
    }))
  );
  console.groupEnd();
}

export const perf = { start, mark, getSummary, printBaseline } as const;

// 将 perf 暴露到 window，方便在 DevTools Console 中手动查看
if (typeof window !== "undefined") {
  (window as unknown as Record<string, unknown>).__perf = perf;
}
