import { defineConfig } from "vitest/config";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig(async ({ mode }) => {
  const plugins = [
    react({
      babel: {
        plugins: [
          ["babel-plugin-react-compiler", {}],
        ],
      },
    }),
    tailwindcss(),
  ];

  // 构建产物可视化分析：ANALYZE=true npm run build
  if (process.env.ANALYZE) {
    const { visualizer } = await import("rollup-plugin-visualizer");
    plugins.push(
      visualizer({
        open: true,
        filename: "dist/bundle-stats.html",
        gzipSize: true,
        brotliSize: true,
      }) as any,
    );
  }

  return {
  plugins,
  // Polyfill Node.js globals used by dependencies (e.g. Ketcher).
  // In test mode vitest provides its own process/env — do NOT override
  // NODE_ENV to "production" or react-dom will load its prod bundle
  // and strip React.act, breaking @testing-library/react.
  ...(mode === "test"
    ? {}
    : {
        define: {
          "process.env": JSON.stringify({}),
          "process.env.NODE_ENV": JSON.stringify("production"),
        },
      }),
  test: {
    globals: true,
    environment: "jsdom",
    include: ["src/**/*.test.ts", "src/**/*.test.tsx"],
  },
  // Tauri 开发服务器配置
  server: {
    port: 5173,
    strictPort: true,
  },
  // 构建输出到 dist 目录，供 Tauri 打包使用
  build: {
    outDir: "dist",
    emptyOutDir: true,
    modulePreload: false,
    chunkSizeWarningLimit: 900,
    // Force Rollup to transform CJS require() in mixed ESM/CJS packages (Ketcher, indigo)
    commonjsOptions: {
      include: [/node_modules/],
      transformMixedEsModules: true,
    },
    rollupOptions: {
      output: {
        manualChunks(id) {
          // Skip Vite internals — never move them into vendor chunks
          if (!id.includes("node_modules/")) return undefined;

          if (
            id.includes("node_modules/react/") ||
            id.includes("node_modules/react-dom/") ||
            id.includes("node_modules/scheduler/")
          ) {
            return "vendor-react";
          }
          if (id.includes("node_modules/@tauri-apps")) {
            return "vendor-tauri";
          }
          if (id.includes("node_modules/3dmol")) {
            return "vendor-3dmol";
          }
          if (id.includes("node_modules/plotly.js-basic-dist-min") || id.includes("node_modules/react-plotly.js")) {
            return "vendor-plotly";
          }
          if (id.includes("node_modules/@tiptap") || id.includes("node_modules/prosemirror")) {
            return "vendor-editor-core";
          }
          if (id.includes("node_modules/tiptap-markdown")) {
            return "vendor-editor-markdown";
          }
          if (id.includes("node_modules/react-force-graph-2d") || id.includes("node_modules/d3-")) {
            return "vendor-graph";
          }
          if (id.includes("node_modules/katex") || id.includes("node_modules/remark-math") || id.includes("node_modules/rehype-katex")) {
            return "vendor-math";
          }
          if (id.includes("node_modules/recharts")) {
            return "vendor-charts";
          }
          if (id.includes("node_modules/smiles-drawer")) {
            return "vendor-smiles";
          }
          // Ketcher is lazy-loaded via React.lazy() — do NOT force it into a manual chunk.
          // Doing so pulls Vite's __vitePreload helper into the ketcher chunk,
          // which forces a static import of the 25 MB bundle at startup.
          if (id.includes("node_modules/jcampconverter")) {
            return "vendor-spectroscopy";
          }
          if (id.includes("node_modules/@dnd-kit")) {
            return "vendor-dnd";
          }
          if (id.includes("node_modules/react-markdown")) {
            return "vendor-markdown-render";
          }
          return undefined;
        },
      },
    },
  },
};});
