import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { NoteContentCacheProvider } from "./contexts/NoteContentCache";
import { perf } from "./utils/perf";
import "./index.css";

// Cold startup timing
perf.mark("app-js-start");

// Global error handler for production debugging
window.addEventListener("error", (e) => {
  document.body.innerHTML += `<pre style="color:red;padding:20px;font-size:12px;white-space:pre-wrap;">ERROR: ${e.message}\n${e.filename}:${e.lineno}</pre>`;
});
window.addEventListener("unhandledrejection", (e) => {
  document.body.innerHTML += `<pre style="color:orange;padding:20px;font-size:12px;white-space:pre-wrap;">REJECTION: ${e.reason}</pre>`;
});

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <NoteContentCacheProvider>
      <App />
    </NoteContentCacheProvider>
  </StrictMode>
);

