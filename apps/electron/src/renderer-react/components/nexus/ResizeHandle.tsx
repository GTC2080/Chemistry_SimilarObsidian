import { useState } from "react";

interface ResizeHandleProps {
  onMouseDown: (e: React.MouseEvent) => void;
  side: "left" | "right";
}

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
        marginLeft: side === "right" ? "-3px" : undefined,
        marginRight: side === "left" ? "-3px" : undefined
      }}
    >
      <div
        className="absolute top-0 bottom-0 transition-opacity duration-150"
        style={{
          width: "1.5px",
          left: "50%",
          transform: "translateX(-50%)",
          background: "var(--accent)",
          opacity: hovered ? 0.5 : 0,
          borderRadius: "1px"
        }}
      />
    </div>
  );
}
