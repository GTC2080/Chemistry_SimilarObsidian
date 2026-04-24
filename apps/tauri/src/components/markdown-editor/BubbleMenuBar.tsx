import { memo } from "react";
import { BubbleMenu } from "@tiptap/react/menus";
import type { Editor } from "@tiptap/core";
import type { DisciplineProfile } from "../settings/settingsTypes";
import { useT } from "../../i18n";

interface BubbleMenuBarProps {
  editor: Editor;
  activeDiscipline: DisciplineProfile;
}

export default function BubbleMenuBar({ editor, activeDiscipline }: BubbleMenuBarProps) {
  const t = useT();

  return (
    <BubbleMenu editor={editor} options={{ placement: "top" }}>
      <div
        className="glass-elevated glass-highlight rounded-[12px] flex items-center gap-0.5 px-1.5 py-1 animate-fade-in"
        style={{ boxShadow: "0 8px 32px rgba(0,0,0,0.45)" }}
      >
        <Btn active={editor.isActive("bold")} onClick={() => editor.chain().focus().toggleBold().run()} label="B" title={t("editor.bold")} bold />
        <Btn active={editor.isActive("italic")} onClick={() => editor.chain().focus().toggleItalic().run()} label="I" title={t("editor.italic")} italic />
        <Btn active={editor.isActive("strike")} onClick={() => editor.chain().focus().toggleStrike().run()} label="S" title={t("editor.strikethrough")} strike />
        <Btn active={editor.isActive("code")} onClick={() => editor.chain().focus().toggleCode().run()} label="<>" title={t("editor.code")} mono />
        <Sep />
        <Btn active={editor.isActive("heading", { level: 1 })} onClick={() => editor.chain().focus().toggleHeading({ level: 1 }).run()} label="H1" title={t("editor.h1")} />
        <Btn active={editor.isActive("heading", { level: 2 })} onClick={() => editor.chain().focus().toggleHeading({ level: 2 }).run()} label="H2" title={t("editor.h2")} />
        <Btn active={editor.isActive("heading", { level: 3 })} onClick={() => editor.chain().focus().toggleHeading({ level: 3 }).run()} label="H3" title={t("editor.h3")} />
        <Sep />
        <Btn active={editor.isActive("bulletList")} onClick={() => editor.chain().focus().toggleBulletList().run()} label="•" title={t("editor.list")} />
        <Btn active={editor.isActive("blockquote")} onClick={() => editor.chain().focus().toggleBlockquote().run()} label="❝" title={t("editor.quote")} />
        <Btn active={editor.isActive("taskList")} onClick={() => editor.chain().focus().toggleTaskList().run()} label="☑" title={t("editor.todo")} />
        <Sep />
        <Btn
          active={editor.isActive("databaseBlock")}
          onClick={() => editor.chain().focus().insertDatabaseBlock().run()}
          label="DB"
          title={t("editor.insertDb")}
          mono
        />
        {activeDiscipline === "chemistry" && (
          <Btn
            active={editor.isActive("stoichiometryBlock")}
            onClick={() => editor.chain().focus().insertStoichiometryBlock().run()}
            label="ST"
            title={t("editor.insertStoich")}
            mono
          />
        )}
      </div>
    </BubbleMenu>
  );
}

/** Separator for BubbleMenu */
function Sep() {
  return (
    <div
      className="w-px h-4 mx-0.5"
      style={{ background: "var(--separator-light)" }}
    />
  );
}

/** BubbleMenu button */
const Btn = memo(function Btn({
  active,
  onClick,
  label,
  title,
  bold,
  italic,
  strike,
  mono,
}: {
  active: boolean;
  onClick: () => void;
  label: string;
  title: string;
  bold?: boolean;
  italic?: boolean;
  strike?: boolean;
  mono?: boolean;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      title={title}
      className={`px-2 py-1 rounded-[8px] text-[12px] cursor-pointer transition-colors duration-150 ${
        active
          ? "text-white"
          : "hover:bg-[var(--sidebar-hover)]"
      }`}
      style={{
        background: active ? "var(--accent)" : "transparent",
        color: active ? "#fff" : "var(--text-secondary)",
        fontWeight: bold ? 700 : undefined,
        fontStyle: italic ? "italic" : undefined,
        textDecoration: strike ? "line-through" : undefined,
        fontFamily: mono
          ? '"SF Mono", "Fira Code", Consolas, monospace'
          : undefined,
      }}
    >
      {label}
    </button>
  );
});
