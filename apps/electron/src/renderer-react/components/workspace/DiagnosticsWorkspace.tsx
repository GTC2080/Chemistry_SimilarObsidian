import { useEffect, useState } from "react";
import {
  exportSupportBundle,
  getBootstrapInfo,
  getRebuildStatus,
  getRuntimeSummary,
  getSessionStatus,
  type HostBootstrapInfo,
  type HostRebuildStatus,
  type HostRuntimeSummary,
  type HostSessionStatus
} from "../../lib/host-shell";
import { pickDirectory } from "../../lib/desktop-shell";
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
  formatMsTimestamp,
  formatNsTimestamp,
  joinPath
} from "./ToolingScaffold";

interface DiagnosticsWorkspaceProps {
  visible: boolean;
}

function defaultBundleFileName() {
  const now = new Date();
  const stamp = [
    now.getFullYear(),
    String(now.getMonth() + 1).padStart(2, "0"),
    String(now.getDate()).padStart(2, "0"),
    "-",
    String(now.getHours()).padStart(2, "0"),
    String(now.getMinutes()).padStart(2, "0"),
    String(now.getSeconds()).padStart(2, "0")
  ].join("");
  return `support-bundle-${stamp}.json`;
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

function readableLifecycle(value: string) {
  if (value === "ready") {
    return "运行中";
  }
  if (value === "starting") {
    return "启动中";
  }
  if (value === "closing") {
    return "关闭中";
  }
  return value || "未知";
}

export default function DiagnosticsWorkspace({
  visible
}: DiagnosticsWorkspaceProps) {
  const [bootstrap, setBootstrap] = useState<HostBootstrapInfo | null>(null);
  const [runtime, setRuntime] = useState<HostRuntimeSummary | null>(null);
  const [session, setSession] = useState<HostSessionStatus | null>(null);
  const [rebuild, setRebuild] = useState<HostRebuildStatus | null>(null);
  const [loading, setLoading] = useState(false);
  const [exporting, setExporting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [lastExportPath, setLastExportPath] = useState<string | null>(null);

  async function refresh() {
    setLoading(true);
    const [bootstrapEnvelope, runtimeEnvelope, sessionEnvelope, rebuildEnvelope] = await Promise.all([
      getBootstrapInfo(),
      getRuntimeSummary(),
      getSessionStatus(),
      getRebuildStatus()
    ]);

    if (!bootstrapEnvelope?.ok || !bootstrapEnvelope.data) {
      setError(bootstrapEnvelope?.error?.message ?? "读取 bootstrap 信息失败。");
      setLoading(false);
      return;
    }

    if (!runtimeEnvelope?.ok || !runtimeEnvelope.data) {
      setError(runtimeEnvelope?.error?.message ?? "读取 runtime summary 失败。");
      setLoading(false);
      return;
    }

    setBootstrap(bootstrapEnvelope.data);
    setRuntime(runtimeEnvelope.data);
    setSession(sessionEnvelope?.ok && sessionEnvelope.data ? sessionEnvelope.data : null);
    setRebuild(rebuildEnvelope?.ok && rebuildEnvelope.data ? rebuildEnvelope.data : null);
    setError(null);
    setLoading(false);
  }

  useEffect(() => {
    if (!visible) {
      return;
    }

    void refresh();
  }, [visible]);

  async function handleExportBundle() {
    const selectedDirectory = await pickDirectory();
    if (!selectedDirectory) {
      return;
    }

    setExporting(true);
    const outputPath = joinPath(selectedDirectory, defaultBundleFileName());
    const envelope = await exportSupportBundle(outputPath);
    if (!envelope?.ok || !envelope.data) {
      setError(envelope?.error?.message ?? "导出 support bundle 失败。");
      setExporting(false);
      return;
    }

    setLastExportPath(envelope.data.outputPath);
    setError(null);
    setExporting(false);
    await refresh();
  }

  return (
    <ToolWorkspaceShell
      sidebar={
        <>
          <ToolSection
            title="支持与排查"
            subtitle="低频入口：查看当前状态或导出支持包。"
            action={<ToolActionButton onClick={() => void refresh()} disabled={loading}>刷新</ToolActionButton>}
          >
            <div className="space-y-3">
              <ToolActionButton onClick={() => void handleExportBundle()} disabled={exporting} tone="primary">
                {exporting ? "导出中…" : "导出支持包"}
              </ToolActionButton>
              {lastExportPath ? (
                <div className="text-[11px] leading-5 break-all text-[var(--text-quaternary)]">
                  最近导出：
                  <br />
                  {lastExportPath}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">尚未导出支持包。</div>
              )}
            </div>
          </ToolSection>

          <ToolSection title="当前状态" subtitle="默认只显示用户和排障最常用的信息。">
            {runtime ? (
              <div className="space-y-2">
                <ToolMetaGrid
                  items={[
                    { label: "应用", value: readableLifecycle(runtime.lifecycle_state) },
                    { label: "索引", value: readableIndexState(runtime.kernel_runtime.index_state) },
                    { label: "当前仓库", value: runtime.session.active_vault_path || "未打开" }
                  ]}
                />
              </div>
            ) : (
              <div className="text-[12px] text-[var(--text-quaternary)]">等待宿主 summary。</div>
            )}
          </ToolSection>
        </>
      }
    >
      {error ? (
        <div className="p-6">
          <ToolErrorBanner message={error} />
        </div>
      ) : !bootstrap || !runtime ? (
        <ToolEmptyState
          title="诊断入口"
          description="这里用于查看当前宿主状态，并在需要排查时导出支持包。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title="诊断与支持包"
            subtitle={runtime.session.active_vault_path || "当前未打开仓库"}
            badges={
              <>
                <ToolBadge label={readableLifecycle(runtime.lifecycle_state)} />
                <ToolBadge label={`索引 ${readableIndexState(runtime.kernel_runtime.index_state)}`} />
                <ToolBadge label={runtime.kernel_binding.attached ? "内核已连接" : "内核未连接"} />
              </>
            }
          />

          <ToolBody>
            {loading ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在刷新诊断信息…</div>
            ) : null}

            <ToolDetailSection title="健康摘要" subtitle="默认展示当前工作状态，不暴露底层 binding 和 raw runtime 字段。">
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="应用状态" value={readableLifecycle(runtime.lifecycle_state)} />
                <ToolMetric label="索引状态" value={readableIndexState(runtime.kernel_runtime.index_state)} hint={`${runtime.kernel_runtime.indexed_note_count ?? 0} 篇笔记`} />
                <ToolMetric label="恢复任务" value={String(runtime.kernel_runtime.pending_recovery_ops ?? 0)} />
              </div>
            </ToolDetailSection>

            <ToolDetailSection title="支持包" subtitle="遇到异常时导出，用于后续排查。Renderer 不解析内部格式。">
              <div className="flex flex-wrap items-center gap-3">
                <ToolActionButton onClick={() => void handleExportBundle()} disabled={exporting} tone="primary">
                  {exporting ? "导出中…" : "导出支持包"}
                </ToolActionButton>
                {lastExportPath ? (
                  <span className="text-[12px] break-all text-[var(--text-quaternary)]">{lastExportPath}</span>
                ) : (
                  <span className="text-[12px] text-[var(--text-quaternary)]">尚未导出。</span>
                )}
              </div>
            </ToolDetailSection>

            <ToolDevDetails subtitle="包含 bootstrap、安全基线、运行模式和 preload 边界。">
              <ToolMetaGrid
                items={[
                  { label: "host_version", value: bootstrap.host_version },
                  { label: "run_mode", value: bootstrap.run_mode },
                  { label: "platform", value: bootstrap.platform },
                  { label: "electron", value: bootstrap.versions.electron },
                  { label: "chrome", value: bootstrap.versions.chrome },
                  { label: "node", value: bootstrap.versions.node },
                  { label: "context_isolation", value: String(bootstrap.security.contextIsolation) },
                  { label: "sandbox", value: String(bootstrap.security.sandbox) }
                ]}
              />
            </ToolDevDetails>

            <ToolDevDetails title="Runtime / Session 详情" subtitle="宿主生命周期和单 vault session 的 raw summary。">
              <ToolMetaGrid
                items={[
                  { label: "lifecycle_state", value: runtime.lifecycle_state },
                  { label: "kernel_session_state", value: runtime.kernel_runtime.session_state },
                  { label: "index_state", value: runtime.kernel_runtime.index_state },
                  { label: "main_window_visible", value: String(runtime.main_window.visible) },
                  { label: "session_state", value: session?.state || runtime.session.state },
                  { label: "active_vault_path", value: session?.active_vault_path || runtime.session.active_vault_path || "(none)" },
                  { label: "last_window_event", value: runtime.last_window_event?.kind || "(none)" },
                  { label: "last_window_event_at", value: runtime.last_window_event ? formatMsTimestamp(runtime.last_window_event.at_ms) : "未记录" }
                ]}
              />
            </ToolDevDetails>

            <ToolDevDetails title="Kernel binding / Rebuild 详情" subtitle="真实 Node-API binding 与 rebuild lifecycle 原始字段。">
              <ToolMetaGrid
                items={[
                  { label: "binding_attached", value: String(runtime.kernel_binding.attached) },
                  { label: "binding_failure_code", value: String(runtime.kernel_binding.failure_code ?? "(none)") },
                  { label: "binary_path", value: String(runtime.kernel_binding.binary_path ?? "(none)") },
                  { label: "rebuild_in_flight", value: String(rebuild?.status.inFlight ?? runtime.rebuild.in_flight) },
                  { label: "last_result_code", value: String(rebuild?.status.lastResultCode ?? runtime.rebuild.last_result_code ?? "(none)") },
                  { label: "last_result_at", value: formatNsTimestamp(rebuild?.status.lastResultAtNs ?? runtime.rebuild.last_result_at_ns ?? 0) },
                  { label: "current_generation", value: String(rebuild?.status.currentGeneration ?? runtime.rebuild.current_generation ?? 0) },
                  { label: "last_completed_generation", value: String(rebuild?.status.lastCompletedGeneration ?? runtime.rebuild.last_completed_generation ?? 0) }
                ]}
              />
            </ToolDevDetails>

            {session?.last_error ? (
              <ToolDetailSection title="最近会话错误">
                <ToolErrorBanner
                  message={`${session.last_error.code}: ${session.last_error.message} @ ${formatMsTimestamp(session.last_error.at_ms)}`}
                />
              </ToolDetailSection>
            ) : null}
          </ToolBody>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
