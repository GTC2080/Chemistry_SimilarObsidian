import { useEffect, useState } from "react";
import {
  getRebuildStatus,
  getRuntimeSummary,
  startRebuild,
  waitForRebuild,
  type HostRebuildStatus,
  type HostRuntimeSummary
} from "../../lib/host-shell";
import {
  ToolActionButton,
  ToolBadge,
  ToolBody,
  ToolContentHeader,
  ToolDevDetails,
  ToolDetailSection,
  ToolEmptyState,
  ToolErrorBanner,
  ToolMetaGrid,
  ToolMetric,
  ToolSection,
  ToolWorkspaceShell,
  formatNsTimestamp
} from "./ToolingScaffold";

interface RebuildWorkspaceProps {
  visible: boolean;
}

function readableIndexState(value: string) {
  if (value === "ready") {
    return "就绪";
  }
  if (value === "rebuilding") {
    return "重建中";
  }
  if (value === "catching_up") {
    return "同步中";
  }
  if (value === "unavailable") {
    return "不可用";
  }
  return value || "未知";
}

function readableResult(value: string | null) {
  if (!value) {
    return "尚无结果";
  }
  if (value === "KERNEL_OK") {
    return "成功";
  }
  return "需要检查";
}

export default function RebuildWorkspace({
  visible
}: RebuildWorkspaceProps) {
  const [status, setStatus] = useState<HostRebuildStatus | null>(null);
  const [runtime, setRuntime] = useState<HostRuntimeSummary | null>(null);
  const [loading, setLoading] = useState(false);
  const [runningAction, setRunningAction] = useState<"start" | "wait" | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [lastEvent, setLastEvent] = useState<string | null>(null);

  async function refresh() {
    setLoading(true);
    const [statusEnvelope, runtimeEnvelope] = await Promise.all([
      getRebuildStatus(),
      getRuntimeSummary()
    ]);

    if (!statusEnvelope?.ok || !statusEnvelope.data) {
      setError(statusEnvelope?.error?.message ?? "读取 rebuild status 失败。");
      setLoading(false);
      return;
    }

    setStatus(statusEnvelope.data);
    setRuntime(runtimeEnvelope?.ok && runtimeEnvelope.data ? runtimeEnvelope.data : null);
    setError(null);
    setLoading(false);
  }

  useEffect(() => {
    if (!visible) {
      return;
    }

    void refresh();
    const timer = window.setInterval(() => {
      void refresh();
    }, 3000);

    return () => {
      window.clearInterval(timer);
    };
  }, [visible]);

  async function handleStart() {
    setRunningAction("start");
    const envelope = await startRebuild();
    if (!envelope?.ok || !envelope.data) {
      setError(envelope?.error?.message ?? "启动 rebuild 失败。");
      setRunningAction(null);
      return;
    }

    setLastEvent(envelope.data.result === "started" ? "重建已启动。" : `重建请求：${envelope.data.result}`);
    setRunningAction(null);
    await refresh();
  }

  async function handleWait() {
    setRunningAction("wait");
    const envelope = await waitForRebuild(15000);
    if (!envelope?.ok || !envelope.data) {
      setError(envelope?.error?.message ?? "等待 rebuild 结果失败。");
      setRunningAction(null);
      return;
    }

    setLastEvent(envelope.data.result === "completed" ? "重建已完成。" : `等待结果：${envelope.data.result}`);
    setRunningAction(null);
    await refresh();
  }

  return (
    <ToolWorkspaceShell
      sidebar={
        <>
          <ToolSection title="索引重建" subtitle="低频维护入口：启动重建、等待完成或刷新状态。">
            <div className="space-y-3">
              <ToolActionButton onClick={() => void handleStart()} disabled={runningAction !== null} tone="primary">
                {runningAction === "start" ? "启动中…" : "重建索引"}
              </ToolActionButton>
              <ToolActionButton onClick={() => void handleWait()} disabled={runningAction !== null}>
                {runningAction === "wait" ? "等待中…" : "等待完成"}
              </ToolActionButton>
              <ToolActionButton onClick={() => void refresh()} disabled={loading}>
                {loading ? "刷新中…" : "刷新状态"}
              </ToolActionButton>
            </div>
          </ToolSection>

          <ToolSection title="最近事件" subtitle="只显示宿主可见的重建结果。">
            <div className="text-[12px] leading-6 text-[var(--text-quaternary)]">
              {lastEvent || "尚未触发重建。"}
            </div>
          </ToolSection>
        </>
      }
    >
      {error ? (
        <div className="p-6">
          <ToolErrorBanner message={error} />
        </div>
      ) : !status ? (
        <ToolEmptyState
          title="索引重建"
          description="这里用于手动触发索引重建，并观察最近一次重建结果。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title="索引重建"
            subtitle="用于重新生成派生索引；不会改变 vault 文件真相。"
            badges={
              <>
                <ToolBadge label={status.status.inFlight ? "重建中" : "空闲"} />
                <ToolBadge label={`索引 ${readableIndexState(status.status.indexState)}`} />
                <ToolBadge label={readableResult(status.status.lastResultCode)} />
              </>
            }
          />

          <ToolBody>
            <ToolDetailSection title="任务摘要" subtitle="默认只展示用户需要知道的重建状态。">
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="当前任务" value={status.status.inFlight ? "重建中" : "空闲"} hint={readableIndexState(status.status.indexState)} />
                <ToolMetric label="最近结果" value={readableResult(status.status.lastResultCode)} hint={status.status.lastResultCode ?? "尚未记录"} />
                <ToolMetric label="最近完成" value={formatNsTimestamp(status.status.lastResultAtNs)} />
              </div>
            </ToolDetailSection>

            <ToolDetailSection title="操作" subtitle="重建是单实例长任务；重复请求由 host 保持稳定语义。">
              <div className="flex flex-wrap items-center gap-3">
                <ToolActionButton onClick={() => void handleStart()} disabled={runningAction !== null || status.status.inFlight} tone="primary">
                  {runningAction === "start" ? "启动中…" : "重建索引"}
                </ToolActionButton>
                <ToolActionButton onClick={() => void handleWait()} disabled={runningAction !== null}>
                  {runningAction === "wait" ? "等待中…" : "等待完成"}
                </ToolActionButton>
                <ToolActionButton onClick={() => void refresh()} disabled={loading}>
                  {loading ? "刷新中…" : "刷新状态"}
                </ToolActionButton>
              </div>
            </ToolDetailSection>

            <ToolDevDetails title="Rebuild status 详情" subtitle="直接映射 host rebuild status，不在 renderer 自建任务队列。">
              <ToolMetaGrid
                items={[
                  { label: "run_mode", value: status.runMode },
                  { label: "adapter_attached", value: String(status.adapterAttached) },
                  { label: "in_flight", value: String(status.status.inFlight) },
                  { label: "has_last_result", value: String(status.status.hasLastResult) },
                  { label: "current_generation", value: String(status.status.currentGeneration) },
                  { label: "last_completed_generation", value: String(status.status.lastCompletedGeneration) },
                  { label: "current_started_at", value: formatNsTimestamp(status.status.currentStartedAtNs) },
                  { label: "last_result_code", value: status.status.lastResultCode ?? "(none)" },
                  { label: "last_result_at", value: formatNsTimestamp(status.status.lastResultAtNs) },
                  { label: "index_state", value: status.status.indexState }
                ]}
              />
            </ToolDevDetails>

            {runtime ? (
              <ToolDevDetails title="Runtime consistency 详情" subtitle="runtime summary 与 rebuild summary 的一致性检查面。">
                <ToolMetaGrid
                  items={[
                    { label: "runtime.lifecycle", value: runtime.lifecycle_state },
                    { label: "runtime.kernel_session", value: runtime.kernel_runtime.session_state },
                    { label: "runtime.index_state", value: runtime.kernel_runtime.index_state },
                    { label: "runtime.rebuild.in_flight", value: String(runtime.rebuild.in_flight) },
                    { label: "runtime.rebuild.last_result_code", value: String(runtime.rebuild.last_result_code ?? "(none)") },
                    { label: "runtime.rebuild.last_result_at", value: formatNsTimestamp(runtime.rebuild.last_result_at_ns ?? 0) }
                  ]}
                />
              </ToolDevDetails>
            ) : null}
          </ToolBody>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
