import { memo } from "react";
import Markdown from "react-markdown";
import remarkMath from "remark-math";
import rehypeKatex from "rehype-katex";
import "katex/contrib/mhchem";
import "katex/dist/katex.min.css";
import SmilesViewer from "../SmilesViewer";
import { useT } from "../../i18n";

const markdownComponents = {
  code({
    className,
    children,
    ...props
  }: React.HTMLAttributes<HTMLElement> & { children?: React.ReactNode }) {
    if (/language-smiles/.test(className || "")) {
      const smiles = String(children).replace(/\n$/, "");
      return <SmilesViewer smiles={smiles} />;
    }

    return (
      <code className={className} {...props}>
        {children}
      </code>
    );
  },
};

// Pre-allocate static plugin arrays to avoid re-creating on every render
const REMARK_MATH_PLUGINS = [remarkMath];
const REHYPE_KATEX_PLUGINS = [[rehypeKatex, { strict: false, trust: true, throwOnError: false }] as [typeof rehypeKatex, object]];
const EMPTY_PLUGINS: never[] = [];

// Fast check: does content contain math delimiters?
const HAS_MATH_RE = /\$[\s\S]+?\$|\\[([{]/;

const ChatBubble = memo(function ChatBubble({
  role,
  content,
  loading,
}: {
  role: "user" | "assistant";
  content: string;
  loading?: boolean;
}) {
  const t = useT();
  const hasMath = role === "assistant" && HAS_MATH_RE.test(content);
  return (
    <div className={`flex ${role === "user" ? "justify-end" : "justify-start"}`}>
      <div
        className="max-w-[90%] rounded-[14px] px-3.5 py-2.5 text-[13px] leading-relaxed"
        style={
          role === "user"
            ? {
                background: "linear-gradient(135deg, #0A84FF 0%, #0070E0 100%)",
                color: "#fff",
                boxShadow: "0 2px 8px rgba(10,132,255,0.2)",
              }
            : {
                background: "var(--subtle-surface)",
                color: "var(--text-primary)",
              }
        }
      >
        {role === "assistant" && loading && !content ? (
          <div className="flex items-center gap-2 py-0.5">
            <div
              className="w-3.5 h-3.5 rounded-full border-[1.5px] animate-spin"
              style={{ borderColor: "rgba(10,132,255,0.15)", borderTopColor: "var(--accent)" }}
            />
            <span className="text-[12px]" style={{ color: "var(--text-tertiary)" }}>
              {t("ai.retrieving")}
            </span>
          </div>
        ) : role === "assistant" ? (
          <div className="ai-markdown">
            <Markdown
              remarkPlugins={hasMath ? REMARK_MATH_PLUGINS : EMPTY_PLUGINS}
              rehypePlugins={hasMath ? REHYPE_KATEX_PLUGINS : EMPTY_PLUGINS}
              components={markdownComponents}
            >
              {content}
            </Markdown>
          </div>
        ) : (
          <span>{content}</span>
        )}
      </div>
    </div>
  );
});

export default ChatBubble;
