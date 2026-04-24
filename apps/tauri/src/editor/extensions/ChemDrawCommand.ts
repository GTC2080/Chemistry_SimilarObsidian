import { Extension } from "@tiptap/core";
import { PluginKey, Plugin } from "@tiptap/pm/state";

export const ChemDrawCommand = Extension.create({
  name: "chemDrawCommand",

  addProseMirrorPlugins() {
    return [
      new Plugin({
        key: new PluginKey("chemDrawCommand"),
        props: {
          handleTextInput(view, from, to, text) {
            const { state } = view;
            const $from = state.doc.resolve(from);
            const textBefore = $from.parent.textBetween(
              Math.max(0, $from.parentOffset - 9),
              $from.parentOffset,
            );
            if ((textBefore + text).endsWith("/chemdraw")) {
              const deleteFrom = from - 8;
              view.dispatch(state.tr.delete(deleteFrom, to));
              window.dispatchEvent(new CustomEvent("open-chemdraw-modal"));
              return true;
            }
            return false;
          },
        },
      }),
    ];
  },
});
