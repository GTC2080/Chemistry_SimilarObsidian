import {
  useCallback,
  useDeferredValue,
  useMemo,
  useRef,
  useState,
} from "react";
import { invoke, Channel } from "@tauri-apps/api/core";
import { useT } from "../i18n";

export interface ChatMessage {
  id: string;
  role: "user" | "assistant";
  content: string;
}

export interface StreamingAssistantMessage {
  id: string;
  content: string;
  loading: boolean;
}

function createMessageId(prefix: string) {
  return `${prefix}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
}

export function useAIChatStream({ activeNoteId }: { activeNoteId?: string }) {
  const t = useT();
  const [history, setHistory] = useState<ChatMessage[]>([]);
  const [streamingMessage, setStreamingMessage] = useState<StreamingAssistantMessage | null>(null);
  const [input, setInput] = useState("");
  const [streaming, setStreaming] = useState(false);
  const streamingContentRef = useRef("");
  const deferredStreamingContent = useDeferredValue(streamingMessage?.content ?? "");

  // --- 帧级合并：收集 token 后在下一帧一次性刷新 ---
  const rafIdRef = useRef(0);
  const pendingFlushRef = useRef(false);

  const flushStreamingContent = useCallback(() => {
    pendingFlushRef.current = false;
    const content = streamingContentRef.current;
    setStreamingMessage(prev =>
      prev ? { ...prev, content, loading: false } : prev
    );
  }, []);

  const scheduleFlush = useCallback(() => {
    if (pendingFlushRef.current) return;
    pendingFlushRef.current = true;
    rafIdRef.current = requestAnimationFrame(flushStreamingContent);
  }, [flushStreamingContent]);

  // --- 取消请求 ---
  const abortControllerRef = useRef<AbortController | null>(null);

  const renderedMessages = useMemo(() => {
    if (!streamingMessage) {
      return history;
    }

    return [
      ...history,
      {
        id: streamingMessage.id,
        role: "assistant" as const,
        content: deferredStreamingContent,
      },
    ];
  }, [deferredStreamingContent, history, streamingMessage]);

  const clearConversation = useCallback(() => {
    setHistory([]);
    setStreamingMessage(null);
    streamingContentRef.current = "";
  }, []);

  const finalizeAssistantMessage = useCallback((content: string) => {
    // 取消挂起的 rAF
    cancelAnimationFrame(rafIdRef.current);
    pendingFlushRef.current = false;

    setHistory(prev => [
      ...prev,
      {
        id: createMessageId("assistant"),
        role: "assistant",
        content,
      },
    ]);
    setStreamingMessage(null);
    streamingContentRef.current = "";
  }, []);

  const cancelStreaming = useCallback(() => {
    abortControllerRef.current?.abort();
    abortControllerRef.current = null;
    cancelAnimationFrame(rafIdRef.current);
    pendingFlushRef.current = false;

    // 保留已收到的内容
    const partialContent = streamingContentRef.current;
    if (partialContent) {
      finalizeAssistantMessage(partialContent + "\n\n[已取消]");
    } else {
      setStreamingMessage(null);
      streamingContentRef.current = "";
    }
    setStreaming(false);
  }, [finalizeAssistantMessage]);

  const handleSubmit = useCallback(async () => {
    const question = input.trim();
    if (!question || streaming) {
      return;
    }

    const userMessage: ChatMessage = {
      id: createMessageId("user"),
      role: "user",
      content: question,
    };
    const streamingId = createMessageId("stream");

    // 创建 AbortController 用于取消
    const controller = new AbortController();
    abortControllerRef.current = controller;

    setInput("");
    setStreaming(true);
    setHistory(prev => [...prev, userMessage]);
    setStreamingMessage({
      id: streamingId,
      content: "",
      loading: true,
    });
    streamingContentRef.current = "";

    try {
      const channel = new Channel<string>();
      channel.onmessage = chunk => {
        // 如果已取消，忽略后续 chunk
        if (controller.signal.aborted) return;
        streamingContentRef.current += chunk;
        // 帧级合并：不立即 setState，等下一帧统一刷新
        scheduleFlush();
      };

      await invoke("ask_vault", {
        question,
        activeNoteId: activeNoteId ?? null,
        onEvent: channel,
      });

      if (!controller.signal.aborted) {
        finalizeAssistantMessage(streamingContentRef.current);
      }
    } catch (cause) {
      if (!controller.signal.aborted) {
        const message = `${t("ai.error")}${cause instanceof Error ? cause.message : String(cause)}`;
        finalizeAssistantMessage(message);
      }
    } finally {
      abortControllerRef.current = null;
      setStreaming(false);
    }
  }, [activeNoteId, finalizeAssistantMessage, input, streaming, scheduleFlush]);

  const handleKeyDown = useCallback((event: React.KeyboardEvent) => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      void handleSubmit();
    }
    // Escape 取消
    if (event.key === "Escape" && streaming) {
      event.preventDefault();
      cancelStreaming();
    }
  }, [handleSubmit, streaming, cancelStreaming]);

  const handleInputChange = useCallback((event: React.ChangeEvent<HTMLTextAreaElement>) => {
    setInput(event.target.value);
    const element = event.target;
    element.style.height = "auto";
    element.style.height = `${Math.min(element.scrollHeight, 96)}px`;
  }, []);

  return {
    renderedMessages,
    streamingMessage,
    input,
    setInput,
    streaming,
    handleSubmit,
    handleKeyDown,
    handleInputChange,
    clearConversation,
    cancelStreaming,
  };
}
