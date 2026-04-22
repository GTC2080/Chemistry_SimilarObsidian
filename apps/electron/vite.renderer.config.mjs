import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  root: "src/renderer-react",
  base: "./",
  plugins: [
    react(),
    tailwindcss()
  ],
  build: {
    outDir: "../../build/renderer-react",
    emptyOutDir: true
  }
});
