import { lazy, Suspense, useState, useEffect, useRef, useCallback, useMemo, useTransition } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { GraphNode, GraphLink, NoteInfo } from "../../types";
import { perf } from "../../utils/perf";
import { useT } from "../../i18n";
import "./global-graph-modal.css";
const GlobalGraphCanvas = lazy(() => import("./GlobalGraphCanvas"));

/** Kernel-backed get_enriched_graph_data command 返回的增强版图谱数据 */
interface EnrichedGraphData {
  nodes: GraphNode[];
  links: GraphLink[];
  neighbors: Record<string, string[]>;
  linkPairs: string[];
}

// ── 图谱采样阈值 ──
const SAMPLE_THRESHOLD = 500;   // 超过此节点数时默认采样展示
const SAMPLE_TOP_N = 300;       // 采样时保留连接数最多的 N 个节点

/** 对大图做采样：保留连接数最高的 topN 个节点及其之间的连线 */
function sampleGraph(data: EnrichedGraphData, topN: number): EnrichedGraphData {
  // 按连接数排序取 topN
  const connectionCount = new Map<string, number>();
  for (const [id, arr] of Object.entries(data.neighbors)) {
    connectionCount.set(id, arr.length);
  }
  const sortedIds = [...connectionCount.entries()]
    .sort((a, b) => b[1] - a[1])
    .slice(0, topN)
    .map(([id]) => id);
  const keepSet = new Set(sortedIds);

  const nodes = data.nodes.filter(n => keepSet.has(n.id));
  const nodeIdSet = new Set(nodes.map(n => n.id));
  const links = data.links.filter(l => nodeIdSet.has(l.source) && nodeIdSet.has(l.target));

  // 重建 neighbors 和 linkPairs
  const neighbors: Record<string, string[]> = {};
  const linkPairs: string[] = [];
  for (const link of links) {
    (neighbors[link.source] ??= []).push(link.target);
    (neighbors[link.target] ??= []).push(link.source);
    linkPairs.push(`${link.source}->${link.target}`);
    linkPairs.push(`${link.target}->${link.source}`);
  }

  return { nodes, links, neighbors, linkPairs };
}

// ── 布局缓存（sessionStorage，关闭标签页后失效） ──
const LAYOUT_CACHE_KEY = "graph-layout-cache";

interface LayoutCache {
  /** 数据版本 key: `${nodeCount}:${linkCount}` */
  version: string;
  /** 节点位置 */
  positions: Record<string, { x: number; y: number }>;
}

function saveLayoutCache(version: string, positions: Record<string, { x: number; y: number }>) {
  try {
    const cache: LayoutCache = { version, positions };
    sessionStorage.setItem(LAYOUT_CACHE_KEY, JSON.stringify(cache));
  } catch { /* sessionStorage 可能满 */ }
}

function loadLayoutCache(version: string): Record<string, { x: number; y: number }> | null {
  try {
    const raw = sessionStorage.getItem(LAYOUT_CACHE_KEY);
    if (!raw) return null;
    const cache: LayoutCache = JSON.parse(raw);
    if (cache.version !== version) return null;
    return cache.positions;
  } catch {
    return null;
  }
}

interface GlobalGraphModalProps {
  open: boolean;
  onClose: () => void;
  /** 双击真实节点后跳转到该笔记 */
  onNavigate: (note: NoteInfo) => void;
  /** 当前所有笔记列表，用于双击时查找完整 NoteInfo */
  notes: NoteInfo[];
}

/** 运行时节点类型（force-graph 会注入 x/y 坐标） */
interface RuntimeNode extends GraphNode {
  x?: number;
  y?: number;
}

export default function GlobalGraphModal({ open, onClose, onNavigate, notes }: GlobalGraphModalProps) {
  const t = useT();
  const [fullGraphData, setFullGraphData] = useState<EnrichedGraphData | null>(null);
  const [sampled, setSampled] = useState(false);   // true = 当前展示的是采样子图
  const [showAll, setShowAll] = useState(false);    // 用户点了"显示全部"
  const [loading, startLoadTransition] = useTransition();
  const [hoveredNode, setHoveredNode] = useState<RuntimeNode | null>(null);
  const [dimensions, setDimensions] = useState({ width: 0, height: 0 });
  const containerRef = useRef<HTMLDivElement>(null);

  // 用 notes 的内容指纹判断是否需要重新拉取图谱
  const notesFingerprint = useMemo(() => {
    let hash = 2166136261;
    for (const n of notes) {
      for (let i = 0; i < n.id.length; i++) {
        hash ^= n.id.charCodeAt(i);
        hash = Math.imul(hash, 16777619);
      }
      hash ^= n.updated_at;
      hash = Math.imul(hash, 16777619);
    }
    return `${notes.length}:${hash >>> 0}`;
  }, [notes]);

  const cachedFingerprintRef = useRef("");

  useEffect(() => {
    if (!open) return;
    if (fullGraphData && notesFingerprint === cachedFingerprintRef.current) return;

    setShowAll(false);
    startLoadTransition(async () => {
      const endGraph = perf.start("graph-open");
      try {
        const data = await invoke<EnrichedGraphData>("get_enriched_graph_data");

        // 尝试恢复布局缓存（用节点+连线数量 + 首几个节点 ID 做版本号）
        const versionParts = [data.nodes.length, data.links.length];
        for (let i = 0; i < Math.min(5, data.nodes.length); i++) {
          versionParts.push(data.nodes[i].id.length);
        }
        const version = versionParts.join(":");
        const cached = loadLayoutCache(version);
        if (cached) {
          for (const node of data.nodes) {
            const pos = cached[node.id];
            if (pos) {
              (node as RuntimeNode).x = pos.x;
              (node as RuntimeNode).y = pos.y;
            }
          }
        }

        setFullGraphData(data);
        setSampled(data.nodes.length > SAMPLE_THRESHOLD);
        cachedFingerprintRef.current = notesFingerprint;
        endGraph();
      } catch (e) {
        console.error(t("common.graphLoadFailed"), e);
      }
    });
  }, [open, notesFingerprint]);

  // 根据采样状态选择展示数据
  const graphData = useMemo(() => {
    if (!fullGraphData) return null;
    if (showAll || fullGraphData.nodes.length <= SAMPLE_THRESHOLD) return fullGraphData;
    return sampleGraph(fullGraphData, SAMPLE_TOP_N);
  }, [fullGraphData, showAll]);

  // 关闭时保存布局位置
  const savePositions = useCallback(() => {
    if (!fullGraphData) return;
    const version = `${fullGraphData.nodes.length}:${fullGraphData.links.length}`;
    const positions: Record<string, { x: number; y: number }> = {};
    for (const node of fullGraphData.nodes) {
      const rn = node as RuntimeNode;
      if (rn.x != null && rn.y != null) {
        positions[node.id] = { x: rn.x, y: rn.y };
      }
    }
    if (Object.keys(positions).length > 0) {
      saveLayoutCache(version, positions);
    }
  }, [fullGraphData]);

  const handleClose = useCallback(() => {
    savePositions();
    onClose();
  }, [savePositions, onClose]);

  // ResizeObserver 自适应尺寸（节流避免高频 re-render）
  useEffect(() => {
    if (!open || !containerRef.current) return;
    const el = containerRef.current;
    let rafId = 0;
    const ro = new ResizeObserver(entries => {
      cancelAnimationFrame(rafId);
      rafId = requestAnimationFrame(() => {
        for (const entry of entries) {
          const { width, height } = entry.contentRect;
          setDimensions({ width, height });
        }
      });
    });
    ro.observe(el);
    // 初始尺寸
    setDimensions({ width: el.clientWidth, height: el.clientHeight });
    return () => {
      cancelAnimationFrame(rafId);
      ro.disconnect();
    };
  }, [open]);

  // 从 Rust 预计算数据构建前端索引（轻量转换，无重计算）
  const neighborMap = useMemo(() => {
    if (!graphData) return new Map<string, Set<string>>();
    const map = new Map<string, Set<string>>();
    for (const [id, arr] of Object.entries(graphData.neighbors)) {
      map.set(id, new Set(arr));
    }
    return map;
  }, [graphData]);

  const linkSet = useMemo(() => {
    if (!graphData) return new Set<string>();
    return new Set(graphData.linkPairs);
  }, [graphData]);

  // O(1) 笔记查找索引（依赖 props.notes，无法迁移到 Rust）
  const noteMap = useMemo(() => {
    const map = new Map<string, NoteInfo>();
    for (const n of notes) map.set(n.id, n);
    return map;
  }, [notes]);

  // 双击节点 → 导航
  const handleNodeClick = useCallback((node: RuntimeNode) => {
    if (node.ghost) return;
    const noteInfo = noteMap.get(node.id);
    if (noteInfo) {
      savePositions();
      onNavigate(noteInfo);
      onClose();
    }
  }, [noteMap, onNavigate, onClose, savePositions]);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center" onClick={handleClose}>
      {/* Backdrop */}
      <div className="absolute inset-0 global-graph-backdrop" />

      {/* Modal container */}
      <div
        className="animate-fade-in relative w-[calc(100%-48px)] h-[calc(100%-48px)] max-w-[1400px] rounded-2xl overflow-hidden global-graph-modal"
        onClick={e => e.stopPropagation()}
      >

        {/* Header */}
        <div className="flex items-center justify-between px-5 py-3 global-graph-header">
          <div className="flex items-center gap-2.5">
            <svg className="w-4 h-4 global-graph-accent-icon"
              viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="12" cy="5" r="2" /><circle cx="5" cy="19" r="2" /><circle cx="19" cy="19" r="2" />
              <line x1="12" y1="7" x2="5" y2="17" /><line x1="12" y1="7" x2="19" y2="17" />
            </svg>
            <span className="text-[13px] font-medium global-graph-title">
              {t("graph.title")}
            </span>
            {graphData && (
              <span className="text-[11px] ml-2 global-graph-muted">
                {t("graph.stats", { nodes: graphData.nodes.length, links: graphData.links.length })}
                {sampled && !showAll && fullGraphData && (
                  <> / {fullGraphData.nodes.length} total</>
                )}
              </span>
            )}
            {sampled && !showAll && (
              <button
                type="button"
                onClick={() => setShowAll(true)}
                className="text-[11px] ml-2 px-2 py-0.5 rounded-md transition-colors cursor-pointer"
                style={{ color: "var(--accent)", background: "rgba(10,132,255,0.1)" }}
              >
                Show all nodes
              </button>
            )}
          </div>
          <div className="flex items-center gap-2">
            <div className="flex items-center gap-3 mr-4 text-[11px] global-graph-muted">
              <span className="flex items-center gap-1.5">
                <span className="w-2 h-2 rounded-full global-graph-legend-dot-note" />
                {t("graph.note")}
              </span>
              <span className="flex items-center gap-1.5">
                <span className="w-2 h-2 rounded-full global-graph-legend-dot-ghost" />
                {t("graph.ghost")}
              </span>
              <span className="flex items-center gap-1.5">
                <span className="w-4 h-[1.5px] rounded-full" style={{ background: "rgba(10, 132, 255, 0.6)" }} />
                {t("graph.link")}
              </span>
              <span className="flex items-center gap-1.5">
                <span className="w-4 h-[1.5px] rounded-full" style={{ background: "rgba(48, 209, 88, 0.6)" }} />
                {t("graph.tag")}
              </span>
              <span className="flex items-center gap-1.5">
                <span className="w-4 h-[1.5px] rounded-full" style={{ background: "rgba(175, 130, 255, 0.6)" }} />
                {t("graph.similar")}
              </span>
              <span className="flex items-center gap-1.5">
                <span className="w-4 h-[1.5px] rounded-full" style={{ background: "rgba(255, 255, 255, 0.25)" }} />
                {t("graph.folder")}
              </span>
            </div>
            <button
              type="button"
              onClick={handleClose}
              title={t("graph.close")}
              aria-label={t("graph.close")}
              className="w-7 h-7 rounded-lg flex items-center justify-center transition-colors cursor-pointer global-graph-close">
              <svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" strokeWidth="1.5">
                <line x1="1" y1="1" x2="9" y2="9" /><line x1="9" y1="1" x2="1" y2="9" />
              </svg>
            </button>
          </div>
        </div>

        {/* Graph canvas area */}
        <div ref={containerRef} className="w-full global-graph-canvas">
          {loading && (
            <div className="flex items-center justify-center h-full gap-3">
              <div className="w-5 h-5 rounded-full border-2 animate-spin global-graph-loading-spinner" />
              <span className="text-[13px] global-graph-tertiary">{t("graph.loading")}</span>
            </div>
          )}

          {!loading && graphData && graphData.nodes.length === 0 && (
            <div className="flex items-center justify-center h-full">
              <p className="text-[13px] global-graph-muted">{t("graph.empty")}</p>
            </div>
          )}

          {!loading && graphData && graphData.nodes.length > 0 && dimensions.width > 0 && (
            <Suspense fallback={<div className="h-full w-full bg-[rgba(255,255,255,0.01)]" />}>
              <GlobalGraphCanvas
                graphData={graphData}
                width={dimensions.width}
                height={dimensions.height}
                hoveredNode={hoveredNode}
                neighborMap={neighborMap}
                linkSet={linkSet}
                onNodeClick={handleNodeClick}
                onNodeHover={setHoveredNode}
              />
            </Suspense>
          )}
        </div>

        {/* Bottom hint */}
        <div className="absolute bottom-3 left-1/2 -translate-x-1/2 flex items-center gap-4 text-[11px] px-3 py-1.5 rounded-lg global-graph-bottom-hint">
          <span>{t("graph.scrollZoom")}</span>
          <span>{t("graph.dragPan")}</span>
          <span>{t("graph.clickNode")}</span>
        </div>
      </div>
    </div>
  );
}
