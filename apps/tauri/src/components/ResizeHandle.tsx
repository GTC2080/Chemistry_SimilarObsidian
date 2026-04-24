import { useState } from "react";

interface ResizeHandleProps {
  onMouseDown: (e: React.MouseEvent) => void;
  /** 手柄位于哪一侧的边缘 */
  side: "left" | "right";
}

/** 可拖拽的分隔条，hover 时显示细线提示，拖拽时高亮 */
export default function ResizeHandle({ onMouseDown, side }: ResizeHandleProps) {
  const [hovered, setHovered] = useState(false);

  return (
    <div
      onMouseDown={onMouseDown}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      className="relative shrink-0"
      style={{
        width: "5px",
        cursor: "col-resize",
        zIndex: 10,
        marginTop: "0",
        marginBottom: "0",
        // 向对应方向偏移，覆盖在边框上，不额外占用布局空间
        marginLeft: side === "right" ? "-3px" : undefined,
        marginRight: side === "left" ? "-3px" : undefined,
      }}
    >
      {/* 拖拽时可见的高亮指示线 */}
      <div
        className="absolute top-0 bottom-0 transition-opacity duration-150"
        style={{
          width: "1.5px",
          left: "50%",
          transform: "translateX(-50%)",
          background: "var(--accent)",
          opacity: hovered ? 0.5 : 0,
          borderRadius: "1px",
        }}
      />
    </div>
  );
}
