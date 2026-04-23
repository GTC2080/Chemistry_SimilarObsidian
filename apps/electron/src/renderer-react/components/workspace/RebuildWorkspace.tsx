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
  ToolContentHeader,
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

    setLastEvent(`rebuild.start -> ${envelope.data.result}`);
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

    setLastEvent(`rebuild.wait -> ${envelope.data.result}`);
    setRunningAction(null);
    await refresh();
  }

  return (
    <ToolWorkspaceShell
      sidebar={
        <>
          <ToolSection title="Rebuild lifecycle" subtitle="这里直接消费 host 的 rebuild start / status / wait。">
            <div className="space-y-3">
              <ToolActionButton onClick={() => void handleStart()} disabled={runningAction !== null} tone="primary">
                {runningAction === "start" ? "启动中…" : "启动 Rebuild"}
              </ToolActionButton>
              <ToolActionButton onClick={() => void handleWait()} disabled={runningAction !== null}>
                {runningAction === "wait" ? "等待中…" : "等待完成"}
              </ToolActionButton>
              <ToolActionButton onClick={() => void refresh()} disabled={loading}>
                {loading ? "刷新中…" : "刷新状态"}
              </ToolActionButton>
            </div>
          </ToolSection>

          <ToolSection title="最近事件" subtitle="保持 host-visible rebuild 收尾语义，不在 renderer 自创队列。">
            <div className="text-[12px] leading-6 text-[var(--text-quaternary)]">
              {lastEvent || "尚未触发 rebuild 操作。"}
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
          title="Rebuild 面板已接入"
          description="这里现在直接消费 rebuild status / start / wait 三个 host 面，并持续跟踪 runtime summary 与 rebuild state 的一致性。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title="Rebuild Lifecycle"
            subtitle={`adapter ${status.adapterAttached ? "attached" : "detached"} · runMode ${status.runMode}`}
            badges={
              <>
                <ToolBadge label={`inFlight ${status.status.inFlight}`} />
                <ToolBadge label={`index ${status.status.indexState}`} />
                <ToolBadge label={`last ${status.status.lastResultCode ?? "none"}`} />
              </>
            }
          />

          <div className="p-6 space-y-6">
            <section>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="In flight" value={String(status.status.inFlight)} />
                <ToolMetric label="Current generation" value={String(status.status.currentGeneration)} />
                <ToolMetric label="Last completed" value={String(status.status.lastCompletedGeneration)} />
              </div>
            </section>

            <section>
              <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">Rebuild status</h2>
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
            </section>

            {runtime ? (
              <section>
                <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">Runtime consistency</h2>
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
              </section>
            ) : null}
          </div>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
