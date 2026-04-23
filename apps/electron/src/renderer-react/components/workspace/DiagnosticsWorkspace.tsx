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
            title="Host 诊断"
            subtitle="这里只消费 host-visible summary，不把 support bundle 内部格式泄漏给 renderer。"
            action={<ToolActionButton onClick={() => void refresh()} disabled={loading}>刷新</ToolActionButton>}
          >
            <div className="space-y-3">
              <ToolActionButton onClick={() => void handleExportBundle()} disabled={exporting} tone="primary">
                {exporting ? "导出中…" : "导出 Support Bundle"}
              </ToolActionButton>
              {lastExportPath ? (
                <div className="text-[11px] leading-5 break-all text-[var(--text-quaternary)]">
                  最近导出：
                  <br />
                  {lastExportPath}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">尚未导出 support bundle。</div>
              )}
            </div>
          </ToolSection>

          <ToolSection title="当前健康摘要" subtitle="基于 runtime / session / rebuild 三条正式宿主面聚合。">
            {runtime ? (
              <div className="space-y-2">
                <ToolMetaGrid
                  items={[
                    { label: "lifecycle", value: runtime.lifecycle_state },
                    { label: "kernel session", value: runtime.kernel_runtime.session_state },
                    { label: "index state", value: runtime.kernel_runtime.index_state },
                    { label: "active vault", value: runtime.session.active_vault_path || "(none)" }
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
          title="诊断面板已接入"
          description="这里会展示 bootstrap/runtime/session/rebuild 的 host-visible summary，并提供 support bundle 导出入口。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title="Diagnostics & Support Bundle"
            subtitle={`runMode ${bootstrap.run_mode} · shell ${bootstrap.shell}`}
            badges={
              <>
                <ToolBadge label={`lifecycle ${runtime.lifecycle_state}`} />
                <ToolBadge label={`index ${runtime.kernel_runtime.index_state}`} />
                <ToolBadge label={`binding ${runtime.kernel_binding.attached ? "attached" : "detached"}`} />
              </>
            }
          />

          <ToolBody>
            {loading ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在刷新 diagnostics summary…</div>
            ) : null}

            <ToolDetailSection title="健康摘要" subtitle="当前 sealed kernel / host / renderer 边界的最小状态面。">
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="API groups" value={String(bootstrap.api_groups.length)} hint={bootstrap.run_mode} />
                <ToolMetric label="Indexed notes" value={String(runtime.kernel_runtime.indexed_note_count ?? 0)} hint={runtime.kernel_runtime.index_state} />
                <ToolMetric label="Recovery ops" value={String(runtime.kernel_runtime.pending_recovery_ops ?? 0)} hint={runtime.kernel_runtime.session_state} />
              </div>
            </ToolDetailSection>

            <ToolDetailSection title="Bootstrap" subtitle="安全基线、运行模式和 preload 边界。">
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
            </ToolDetailSection>

            <ToolDetailSection title="Runtime / Session" subtitle="宿主生命周期和单 vault session 当前状态。">
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
            </ToolDetailSection>

            <ToolDetailSection title="Kernel binding / Rebuild" subtitle="真实 Node-API binding 与 rebuild lifecycle 摘要。">
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
            </ToolDetailSection>

            {session?.last_error ? (
              <ToolDetailSection title="Last session error">
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
