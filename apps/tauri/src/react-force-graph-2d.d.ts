declare module "react-force-graph-2d" {
  import { Component } from "react";

  interface ForceGraphProps {
    graphData?: { nodes: any[]; links: any[] };
    width?: number;
    height?: number;
    backgroundColor?: string;
    nodeRelSize?: number;
    nodeVal?: number | ((node: any) => number);
    nodeLabel?: string | ((node: any) => string);
    nodeColor?: string | ((node: any) => string);
    nodeCanvasObject?: (node: any, ctx: CanvasRenderingContext2D, globalScale: number) => void;
    nodeCanvasObjectMode?: string | ((node: any) => string);
    nodePointerAreaPaint?: (node: any, color: string, ctx: CanvasRenderingContext2D, globalScale: number) => void;
    linkColor?: string | ((link: any) => string);
    linkWidth?: number | ((link: any) => number);
    linkDirectionalParticles?: number | ((link: any) => number);
    linkDirectionalParticleWidth?: number | ((link: any) => number);
    linkDirectionalParticleSpeed?: number | ((link: any) => number);
    linkDirectionalParticleColor?: string | ((link: any) => string);
    linkCanvasObject?: (link: any, ctx: CanvasRenderingContext2D, globalScale: number) => void;
    linkCanvasObjectMode?: string | ((link: any) => string);
    linkCurvature?: number | ((link: any) => number);
    linkLineDash?: number[] | ((link: any) => number[]);
    onNodeClick?: (node: any, event: MouseEvent) => void;
    onNodeHover?: (node: any | null, prevNode: any | null) => void;
    onNodeDragEnd?: (node: any) => void;
    onBackgroundClick?: (event: MouseEvent) => void;
    cooldownTicks?: number;
    cooldownTime?: number;
    d3AlphaDecay?: number;
    d3VelocityDecay?: number;
    d3AlphaMin?: number;
    warmupTicks?: number;
    onEngineStop?: () => void;
    enableNodeDrag?: boolean;
    enableZoomInteraction?: boolean;
    enablePanInteraction?: boolean;
    minZoom?: number;
    maxZoom?: number;
    dagMode?: string;
    dagLevelDistance?: number;
    ref?: any;
  }

  export interface ForceGraphMethods {
    d3Force: (forceName: string, force?: any) => any;
    d3ReheatSimulation: () => void;
    zoom: (zoom?: number, duration?: number) => number;
    centerAt: (x?: number, y?: number, duration?: number) => { x: number; y: number };
    screen2GraphCoords: (x: number, y: number) => { x: number; y: number };
    graph2ScreenCoords: (x: number, y: number) => { x: number; y: number };
  }

  export default class ForceGraph2D extends Component<ForceGraphProps> {}
}
