import { useCallback, useEffect, useMemo, useRef } from "react";
import ForceGraph2D from "react-force-graph-2d";
import type { GraphData, GraphNode } from "../../types";

interface RuntimeNode extends GraphNode {
  x?: number;
  y?: number;
}

interface RuntimeLink {
  source: string | RuntimeNode;
  target: string | RuntimeNode;
  kind?: string;
}

interface GlobalGraphCanvasProps {
  graphData: GraphData;
  width: number;
  height: number;
  hoveredNode: RuntimeNode | null;
  neighborMap: Map<string, Set<string>>;
  linkSet: Set<string>;
  onNodeClick: (node: RuntimeNode) => void;
  onNodeHover: (node: RuntimeNode | null) => void;
}

/* ── 连线颜色配置 ── */
const LINK_COLORS: Record<string, { active: string; dim: string }> = {
  link:       { active: "rgba(10, 132, 255, 0.5)",  dim: "rgba(10, 132, 255, 0.08)" },
  tag:        { active: "rgba(48, 209, 88, 0.45)",  dim: "rgba(48, 209, 88, 0.06)" },
  similarity: { active: "rgba(175, 130, 255, 0.4)", dim: "rgba(175, 130, 255, 0.06)" },
  folder:     { active: "rgba(255, 255, 255, 0.18)", dim: "rgba(255, 255, 255, 0.04)" },
};

function linkKind(link: RuntimeLink): string {
  return link.kind ?? "link";
}

export default function GlobalGraphCanvas({
  graphData,
  width,
  height,
  hoveredNode,
  neighborMap,
  linkSet,
  onNodeClick,
  onNodeHover,
}: GlobalGraphCanvasProps) {
  const fgRef = useRef<any>(null);

  const isHighlighted = useCallback((nodeId: string) => {
    if (!hoveredNode) return true;
    if (hoveredNode.id === nodeId) return true;
    return neighborMap.get(hoveredNode.id)?.has(nodeId) ?? false;
  }, [hoveredNode, neighborMap]);

  const isLinkHighlighted = useCallback((source: string, target: string) => {
    if (!hoveredNode) return true;
    return (hoveredNode.id === source || hoveredNode.id === target) &&
      linkSet.has(`${source}->${target}`);
  }, [hoveredNode, linkSet]);

  /* ── 节点绘制（Obsidian 风格：小圆点） ── */
  const paintNode = useCallback((node: RuntimeNode, ctx: CanvasRenderingContext2D, globalScale: number) => {
    const x = node.x ?? 0;
    const y = node.y ?? 0;
    const highlighted = isHighlighted(node.id);
    const opacity = highlighted ? 1 : 0.12;
    const connections = neighborMap.get(node.id)?.size ?? 0;

    // Obsidian 风格：小节点，2-5px 半径
    const baseSize = Math.min(2 + connections * 0.2, 5);
    const size = hoveredNode?.id === node.id ? baseSize * 1.6 : baseSize;

    let color: string;
    if (node.ghost) {
      color = `rgba(142, 142, 147, ${opacity * 0.4})`;
    } else if (hoveredNode?.id === node.id) {
      color = `rgba(10, 132, 255, ${opacity})`;
    } else {
      color = `rgba(10, 132, 255, ${opacity * 0.75})`;
    }

    // hover 光晕
    if (hoveredNode?.id === node.id) {
      ctx.beginPath();
      ctx.arc(x, y, size + 3, 0, 2 * Math.PI);
      ctx.fillStyle = "rgba(10, 132, 255, 0.1)";
      ctx.fill();
    }

    ctx.beginPath();
    ctx.arc(x, y, size, 0, 2 * Math.PI);
    ctx.fillStyle = color;
    ctx.fill();

    // 标签：放大到 2x 或 hover 时显示
    const showLabel = globalScale > 2 || hoveredNode?.id === node.id ||
      (highlighted && hoveredNode !== null);
    if (showLabel) {
      const fontSize = Math.max(10 / globalScale, 1.8);
      ctx.font = `${fontSize}px -apple-system, BlinkMacSystemFont, sans-serif`;
      ctx.textAlign = "center";
      ctx.textBaseline = "top";
      ctx.fillStyle = highlighted
        ? `rgba(255, 255, 255, ${node.ghost ? 0.35 : 0.75})`
        : "rgba(255, 255, 255, 0.08)";
      ctx.fillText(node.name, x, y + size + 1.5);
    }
  }, [hoveredNode, isHighlighted, neighborMap]);

  const paintPointerArea = useCallback((node: RuntimeNode, color: string, ctx: CanvasRenderingContext2D) => {
    const x = node.x ?? 0;
    const y = node.y ?? 0;
    const connections = neighborMap.get(node.id)?.size ?? 0;
    const size = Math.min(2 + connections * 0.2, 5) + 5; // 比视觉稍大，便于点击
    ctx.beginPath();
    ctx.arc(x, y, size, 0, 2 * Math.PI);
    ctx.fillStyle = color;
    ctx.fill();
  }, [neighborMap]);

  /* ── 力引擎参数（Obsidian 风格：适度散开，聚类自然） ── */
  useEffect(() => {
    if (!fgRef.current) return;
    const fg = fgRef.current;
    const charge = fg.d3Force("charge");
    if (charge) charge.strength(-120).distanceMax(400);
    const link = fg.d3Force("link");
    if (link) link.distance(60).strength(0.3);
    const center = fg.d3Force("center");
    if (center) center.strength(0.08);
  }, [graphData]);

  const nodeCanvasObjectMode = useCallback(() => "replace" as const, []);

  const handleNodeHover = useCallback(
    (node: RuntimeNode | null) => onNodeHover(node),
    [onNodeHover],
  );

  const getLinkColor = useCallback((link: RuntimeLink) => {
    const src = typeof link.source === "object" ? link.source.id : link.source;
    const tgt = typeof link.target === "object" ? link.target.id : link.target;
    const hl = isLinkHighlighted(src, tgt);
    const colors = LINK_COLORS[linkKind(link)] ?? LINK_COLORS.folder;
    return hl ? colors.active : colors.dim;
  }, [isLinkHighlighted]);

  const getLinkWidth = useCallback((link: RuntimeLink) => {
    const k = linkKind(link);
    if (k === "link") return 1;
    if (k === "tag" || k === "similarity") return 0.6;
    return 0.4;
  }, []);

  const getLinkParticles = useCallback((link: RuntimeLink) => {
    return linkKind(link) === "folder" ? 0 : 1;
  }, []);

  const getLinkParticleColor = useCallback((link: RuntimeLink) => {
    const k = linkKind(link);
    if (k === "tag") return "rgba(48, 209, 88, 0.3)";
    if (k === "similarity") return "rgba(175, 130, 255, 0.3)";
    return "rgba(10, 132, 255, 0.3)";
  }, []);

  const noop = useMemo(() => () => {}, []);

  return (
    <ForceGraph2D
      ref={fgRef}
      graphData={graphData}
      width={width}
      height={height}
      backgroundColor="rgba(0,0,0,0)"
      nodeCanvasObject={paintNode}
      nodeCanvasObjectMode={nodeCanvasObjectMode}
      nodePointerAreaPaint={paintPointerArea}
      onNodeClick={onNodeClick}
      onNodeHover={handleNodeHover}
      linkColor={getLinkColor}
      linkWidth={getLinkWidth}
      linkDirectionalParticles={getLinkParticles}
      linkDirectionalParticleWidth={1.2}
      linkDirectionalParticleSpeed={0.003}
      linkDirectionalParticleColor={getLinkParticleColor}
      enableNodeDrag
      enableZoomInteraction
      enablePanInteraction
      warmupTicks={30}
      cooldownTicks={150}
      d3AlphaDecay={0.02}
      d3VelocityDecay={0.3}
      onEngineStop={noop}
      minZoom={0.3}
      maxZoom={10}
    />
  );
}
