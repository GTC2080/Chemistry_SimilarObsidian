import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { LanguageProvider } from "./i18n";
import "./index.css";

window.addEventListener("error", (event) => {
  console.error("renderer.window.error", {
    message: event.message,
    filename: event.filename,
    lineno: event.lineno,
    colno: event.colno
  });
});

window.addEventListener("unhandledrejection", (event) => {
  console.error("renderer.unhandledrejection", {
    reason: event.reason instanceof Error ? event.reason.stack : event.reason
  });
});

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <LanguageProvider language="zh-CN">
      <App />
    </LanguageProvider>
  </StrictMode>
);
