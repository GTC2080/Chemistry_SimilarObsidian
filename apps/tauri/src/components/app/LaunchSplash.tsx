import logoSvg from "../../assets/logo.svg";

/**
 * 启动闪屏 — 在应用加载配置 / 知识库初始化期间显示。
 * 呈现品牌 Logo + 呼吸光晕 + 微妙加载指示器，
 * 让用户立即感知到应用正在启动，而非卡死。
 */
export default function LaunchSplash() {
  return (
    <div className="flex-1 min-h-0 flex flex-col items-center justify-center gap-6 select-none launch-splash">
      {/* Logo with glow */}
      <div className="relative">
        <div className="launch-glow" />
        <img
          src={logoSvg}
          alt=""
          className="w-16 h-16 rounded-2xl relative z-10 launch-logo"
        />
      </div>

      {/* Loading bar */}
      <div className="w-24 h-[2px] rounded-full overflow-hidden launch-track">
        <div className="h-full w-full launch-bar" />
      </div>
    </div>
  );
}
