import { useMemo } from "react";
import type { KeyboardEvent } from "react";
import { NodeViewWrapper } from "@tiptap/react";
import type { NodeViewProps } from "@tiptap/react";
import type {
  DatabaseColumn,
  DatabaseColumnType,
  DatabasePayload,
  DatabaseRow,
} from "../../editor/schema/database";
import { normalizeDatabasePayload } from "../../editor/schema/database";

function createId(prefix: string): string {
  return `${prefix}_${Math.random().toString(36).slice(2, 10)}`;
}

function stopEditorNavigation(e: KeyboardEvent<HTMLInputElement>) {
  if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Enter", "Tab"].includes(e.key)) {
    e.stopPropagation();
  }
}

export default function DatabaseGrid({ node, updateAttributes }: NodeViewProps) {
  const payload = useMemo(
    () => normalizeDatabasePayload(node.attrs as DatabasePayload),
    [node.attrs]
  );
  const { columns, rows } = payload;

  const applyPatch = (nextColumns: DatabaseColumn[], nextRows: DatabaseRow[]) => {
    updateAttributes({ columns: nextColumns, rows: nextRows });
  };

  const updateCell = (rowId: string, columnId: string, value: string) => {
    const nextRows = rows.map(row =>
      row.id === rowId ? { ...row, cells: { ...row.cells, [columnId]: value } } : row
    );
    applyPatch(columns, nextRows);
  };

  const updateColumnName = (columnId: string, name: string) => {
    const nextColumns = columns.map(column =>
      column.id === columnId ? { ...column, name } : column
    );
    applyPatch(nextColumns, rows);
  };

  const updateColumnType = (columnId: string, type: DatabaseColumnType) => {
    const nextColumns = columns.map(column =>
      column.id === columnId ? { ...column, type } : column
    );
    applyPatch(nextColumns, rows);
  };

  const addRow = () => {
    const newRow: DatabaseRow = {
      id: createId("row"),
      cells: Object.fromEntries(columns.map(column => [column.id, ""])),
    };
    applyPatch(columns, [...rows, newRow]);
  };

  const addColumn = () => {
    const newColumn: DatabaseColumn = {
      id: createId("col"),
      name: `Column ${columns.length + 1}`,
      type: "text",
    };
    const nextColumns = [...columns, newColumn];
    const nextRows = rows.map(row => ({
      ...row,
      cells: { ...row.cells, [newColumn.id]: "" },
    }));
    applyPatch(nextColumns, nextRows);
  };

  const removeRow = (rowId: string) => {
    const nextRows = rows.filter(row => row.id !== rowId);
    applyPatch(columns, nextRows);
  };

  const removeColumn = (columnId: string) => {
    if (columns.length <= 1) return;
    const nextColumns = columns.filter(column => column.id !== columnId);
    const nextRows = rows.map(row => {
      const nextCells = { ...row.cells };
      delete nextCells[columnId];
      return { ...row, cells: nextCells };
    });
    applyPatch(nextColumns, nextRows);
  };

  return (
    <NodeViewWrapper className="my-6 border border-[var(--panel-border)] rounded-md overflow-hidden bg-[var(--panel-bg)]" contentEditable={false}>
      <div className="overflow-x-auto">
        <table className="w-full border-collapse table-fixed">
          <thead className="bg-[var(--subtle-surface)] border-b border-[var(--separator-light)]">
            <tr>
              {columns.map((column, colIndex) => (
                <th
                  key={column.id}
                  className={`align-top border-r border-[var(--separator-light)] p-2 ${colIndex === columns.length - 1 ? "border-r-0" : ""}`}
                >
                  <input
                    value={column.name}
                    onChange={e => updateColumnName(column.id, e.target.value)}
                    onKeyDown={stopEditorNavigation}
                    className="w-full bg-transparent text-xs font-medium text-[var(--text-quaternary)] uppercase tracking-wider focus:outline-none focus:ring-0 focus:border focus:border-[var(--accent)] rounded-sm"
                  />
                  <select
                    value={column.type}
                    onChange={e => updateColumnType(column.id, e.target.value as DatabaseColumnType)}
                    onKeyDown={e => e.stopPropagation()}
                    className="mt-1 w-full bg-[var(--subtle-surface-strong)] text-[11px] text-[var(--text-tertiary)] border border-[var(--separator-light)] rounded-sm px-1 py-0.5 focus:outline-none focus:ring-0 focus:border-[var(--accent)]"
                  >
                    <option value="text">text</option>
                    <option value="number">number</option>
                    <option value="select">select</option>
                    <option value="tags">tags</option>
                  </select>
                  <button
                    type="button"
                    onClick={() => removeColumn(column.id)}
                    className="mt-1 text-[10px] text-[var(--text-quaternary)] hover:text-[var(--text-secondary)] transition-colors"
                  >
                    remove
                  </button>
                </th>
              ))}
              <th className="w-[64px] p-1 text-right border-r-0">
                <button
                  type="button"
                  onClick={addColumn}
                  className="text-[var(--text-quaternary)] hover:text-[var(--text-secondary)] text-sm px-2 py-1 transition-colors"
                >
                  +
                </button>
              </th>
            </tr>
          </thead>
          <tbody>
            {rows.map((row, rowIndex) => (
              <tr key={row.id}>
                {columns.map((column, colIndex) => (
                  <td
                    key={`${row.id}-${column.id}`}
                    className={`border-r border-b border-[var(--separator-light)] p-2 text-sm text-[var(--text-secondary)] hover:bg-[var(--sidebar-hover)] ${colIndex === columns.length - 1 ? "border-r-0" : ""} ${rowIndex === rows.length - 1 ? "border-b-0" : ""}`}
                  >
                    <input
                      value={String(row.cells[column.id] ?? "")}
                      onChange={e => updateCell(row.id, column.id, e.target.value)}
                      onKeyDown={stopEditorNavigation}
                      className="w-full bg-transparent text-sm text-[var(--text-secondary)] focus:outline-none focus:ring-0 focus:border focus:border-[var(--accent)] rounded-sm"
                    />
                  </td>
                ))}
                <td className={`border-b border-[var(--separator-light)] ${rowIndex === rows.length - 1 ? "border-b-0" : ""}`}>
                  <button
                    type="button"
                    onClick={() => removeRow(row.id)}
                    className="w-full text-[10px] text-[var(--text-quaternary)] hover:text-[var(--text-secondary)] transition-colors"
                  >
                    remove
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <button
        type="button"
        onClick={addRow}
        className="text-[var(--text-quaternary)] hover:text-[var(--text-secondary)] text-sm p-2 w-full text-left transition-colors border-t border-[var(--separator-light)]"
      >
        + New Row
      </button>
    </NodeViewWrapper>
  );
}
